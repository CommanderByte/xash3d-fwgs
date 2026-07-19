// xash3dpp — cmd_cvar: command tokenizer and dispatcher.
// Legacy reference: engine/common/cmd.c

#include <xash3dpp/private/cmd_cvar/context_impl.hpp>

#include <cstring>

namespace xash::cmd_cvar {

// ---------------------------------------------------------------------------
// Internal tokenizer — splits a single command line into argv[]/argc tokens.
// Writes token strings into a caller-provided scratch buffer.
// Returns the number of tokens parsed, or -1 on buffer overflow.
//
// Quoting rules:
//   "quoted string"  — treated as one token; backslash NOT a standard escape
//   bare token       — terminated by whitespace, ; or \n
//   ; or \n          — command separator; stops tokenising (caller handles it)
// ---------------------------------------------------------------------------
static int tokenize_line(const char *src, const char **argv,
                          int max_argc, char *buf, std::size_t buf_size) noexcept
{
    int argc = 0;
    char *out = buf;
    char *const out_end = buf + buf_size;

    while (*src) {
        // Skip leading whitespace.
        while (*src == ' ' || *src == '\t') ++src;

        // Stop at end-of-line or command separator (caller handles ';').
        if (!*src || *src == ';' || *src == '\n') break;

        // Skip // line comments.
        if (src[0] == '/' && src[1] == '/') break;

        // Check token count overflow.
        if (argc >= max_argc) return -1; // too many tokens

        // Record start of token in scratch buffer.
        if (out >= out_end - 1) return -1; // buffer full
        argv[argc++] = out;

        if (*src == '"') {
            // Quoted token.
            ++src;
            while (*src && *src != '"') {
                if (out < out_end - 1) *out++ = *src;
                ++src;
            }
            if (*src == '"') ++src; // consume closing quote
        } else {
            // Bare token — ends at whitespace, ;, or \n.
            while (*src && *src != ' ' && *src != '\t'
                        && *src != ';'  && *src != '\n') {
                if (out < out_end - 1) *out++ = *src;
                ++src;
            }
        }
        *out++ = '\0';
    }
    return argc;
}

// ---------------------------------------------------------------------------
// cmd_dispatch — tokenize and execute a single command line (or several
// separated by ';').  The helpers use abbreviated C++20 function templates
// so that the Impl type is deduced at each (class-internal) call site
// rather than named directly (Impl is a private nested type).
// ---------------------------------------------------------------------------
static constexpr int k_alias_depth_max = 8;

// Forward declarations (abbreviated templates — mutually recursive).
static void dispatch_cmd(auto &impl, CmdCvarContext &ctx,
                          const char *line, bool is_privileged, int depth) noexcept;
static void execute_tokenized(auto &impl, CmdCvarContext &ctx,
                               bool is_privileged, int depth) noexcept;

static void dispatch_cmd(auto &impl, CmdCvarContext &ctx,
                          const char *line, bool is_privileged, int depth) noexcept
{
    if (!line || !*line) return;

    const char *p = line;
    while (*p) {
        while (*p == ' ' || *p == '\t') ++p;
        if (!*p || *p == '\n') break;

        // Locate end of this command fragment.
        const char *start = p;
        bool in_quotes = false;
        while (*p) {
            if (*p == '"')                               { in_quotes = !in_quotes; ++p; continue; }
            if (!in_quotes && (*p == ';' || *p == '\n'))   break;
            ++p;
        }

        const std::size_t len = static_cast<std::size_t>(p - start);
        if (len == 0) { if (*p) ++p; continue; }

        // Null-terminate a local copy for tokenize_line.
        char line_copy[::xash::limits::cmd_line_max];
        const std::size_t copy_len = (len < sizeof(line_copy) - 1) ? len : sizeof(line_copy) - 2;
        std::memcpy(line_copy, start, copy_len);
        line_copy[copy_len] = '\0';

        // Tokenize into impl scratch.
        char tok_buf[::xash::limits::cmd_line_max];
        impl.tok_argc = tokenize_line(line_copy, impl.tok_argv.data(), impl.k_max_argc,
                                       tok_buf, sizeof(tok_buf));
        if (impl.tok_argc <= 0) { impl.tok_argc = 0; if (*p) ++p; continue; }

        impl.tok_is_privileged = is_privileged;

        // Build tok_argsBuffer = everything after argv[0] (used by cmd_args()).
        {
            const char *after0 = line_copy;
            if (*after0 == '"') {
                ++after0;
                while (*after0 && *after0 != '"') ++after0;
                if (*after0) ++after0;
            } else {
                while (*after0 && *after0 != ' ' && *after0 != '\t') ++after0;
            }
            while (*after0 == ' ' || *after0 == '\t') ++after0;
            ::xash::utilities::strncpy(impl.tok_argsBuffer.data(), after0, impl.tok_argsBuffer.size());
        }

        execute_tokenized(impl, ctx, is_privileged, depth);
        if (*p) ++p;
    }
}

static void execute_tokenized(auto &impl, CmdCvarContext &ctx,
                                bool is_privileged, int depth) noexcept
{
    if (impl.tok_argc == 0) return;
    const char *cmd_name = impl.tok_argv[0];

    // 1. Alias expansion.
    AliasDef *al = impl.alias_map.find(cmd_name);
    if (al) {
        if (depth >= k_alias_depth_max) return;
        dispatch_cmd(impl, ctx, al->value ? al->value : "", is_privileged, depth + 1);
        return;
    }

    // 2. Registered command.
    Command *cmd = impl.cmd_map.find(cmd_name);
    if (cmd) {
        if ((cmd->flags & FCMD_PRIVILEGED) && !is_privileged) return;
        tls_ctx = &ctx;
        if (cmd->ctx_fn)   cmd->ctx_fn(cmd->user); // CommandCtxFn overload
        else if (cmd->fn)  cmd->fn();              // legacy capture-less overload
        tls_ctx = nullptr;
        return;
    }

    // 3. Cvar assignment fallback: "name value".
    if (impl.tok_argc >= 2) {
        Cvar *cv = ctx.cvar_find(cmd_name);
        if (cv)
            ctx.cvar_set_direct(cv, impl.tok_argv[1],
                                 is_privileged ? CvarWriteSource::Console
                                               : CvarWriteSource::StuffCmd);
    }
}

// ---------------------------------------------------------------------------
// Public dispatch API
// ---------------------------------------------------------------------------

void CmdCvarContext::cmd_execute_string(std::string_view text) noexcept
{
    char buf[::xash::limits::cmd_line_max];
    const std::size_t copy_len = text.size() < sizeof(buf) - 1 ? text.size() : sizeof(buf) - 1;
    std::memcpy(buf, text.data(), copy_len);
    buf[copy_len] = '\0';
    dispatch_cmd(*impl_, *this, buf, /*is_privileged=*/true, 0);
}

void CmdCvarContext::cbuf_execute() noexcept
{
    if (impl_->cmd_wait > 0) {
        --impl_->cmd_wait;
        return;
    }

    const std::size_t trusted_count = impl_->cmd_text.size();
    for (std::size_t i = 0; i < trusted_count && !impl_->cmd_text.empty(); ++i) {
        std::string line = std::move(impl_->cmd_text.front());
        impl_->cmd_text.pop_front();
        dispatch_cmd(*impl_, *this, line.c_str(), /*is_privileged=*/true, 0);
        if (impl_->cmd_wait > 0) { --impl_->cmd_wait; return; }
    }

    const bool stuffcmd_trusted =
        impl_->trust_oracle && impl_->trust_oracle->stuffcmd_is_trusted();
    const std::size_t filtered_count = impl_->filteredcmd_text.size();
    for (std::size_t i = 0; i < filtered_count && !impl_->filteredcmd_text.empty(); ++i) {
        std::string line = std::move(impl_->filteredcmd_text.front());
        impl_->filteredcmd_text.pop_front();
        dispatch_cmd(*impl_, *this, line.c_str(), stuffcmd_trusted, 0);
        if (impl_->cmd_wait > 0) { --impl_->cmd_wait; return; }
    }
}

} // namespace xash::cmd_cvar
