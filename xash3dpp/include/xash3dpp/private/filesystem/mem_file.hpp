#pragma once
// xash3dpp — in-memory File implementation for archive lump data
// Legacy reference: filesystem/wad.c  (whole-lump serving with no streaming)
//
// Used by: backends/wad_backend.cpp
// Future:  android_backend (AAsset can be read into memory once),
//          any backend that fully decompresses an entry before serving reads.

#include <xash3dpp/filesystem/file.hpp>

#include <cstring>   // memcpy
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace xash::filesystem {

class MemFile final : public File
{
public:
    explicit MemFile( std::vector<std::byte> data )
        : data_( std::move( data ) )
        , len_( static_cast<FsOffset>( data_.size() ) )
    {}

    FsOffset Read( std::span<std::byte> buf ) override
    {
        if ( pos_ >= len_ ) return 0;
        const FsOffset avail = len_ - pos_;
        const FsOffset n     = static_cast<FsOffset>( buf.size() ) < avail
                                   ? static_cast<FsOffset>( buf.size() ) : avail;
        std::memcpy( buf.data(), data_.data() + pos_, static_cast<std::size_t>( n ) );
        pos_ += n;
        return n;
    }

    FsOffset Write( std::span<const std::byte> /*buf*/ ) override { return -1; }

    FsOffset Seek( FsOffset offset, SeekOrigin origin ) override
    {
        FsOffset newpos;
        switch ( origin )
        {
        case SeekOrigin::Begin:   newpos = offset;         break;
        case SeekOrigin::Current: newpos = pos_ + offset;  break;
        case SeekOrigin::End:     newpos = len_ + offset;  break;
        default:                  return -1;
        }
        if ( newpos < 0 || newpos > len_ ) return -1;
        pos_ = newpos;
        return pos_;
    }

    FsOffset Tell()   const override { return pos_; }
    FsOffset Length() const override { return len_; }
    bool     Eof()    const override { return pos_ >= len_; }
    void     Flush()        override {}

    std::optional<std::string> Gets() override
    {
        if ( pos_ >= len_ ) return std::nullopt;
        std::string line;
        while ( pos_ < len_ )
        {
            const auto c = static_cast<unsigned char>( data_[pos_++] );
            if ( c == '\n' ) break;
            line += static_cast<char>( c );
        }
        return line;
    }

    int Getc() override
    {
        if ( ungetc_ != EOF ) { int c = ungetc_; ungetc_ = EOF; return c; }
        if ( pos_ >= len_ ) return EOF;
        return static_cast<int>( static_cast<unsigned char>( data_[pos_++] ) );
    }

    void UnGetc( int c ) override { ungetc_ = c; }

private:
    std::vector<std::byte> data_;
    FsOffset               len_;
    FsOffset               pos_    = 0;
    int                    ungetc_ = EOF;
};

} // namespace xash::filesystem
