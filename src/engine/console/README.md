# Engine Console

Reserved for future console and logging policy helpers.

Do not move filesystem diagnostics here until `Con_Printf`, `Con_DPrintf`,
`Con_Reportf`, `Log_Printf`, `Sys_Print`, and `Sys_PrintLog` ownership has been
audited. The current plan is to do command/cvar work first.
