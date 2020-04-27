// vim: ft=c

#define ROOT_HELP "LIBCSP Command Interface"

COMMAND_ROOT_FWD(libcsp, ROOT_HELP)

COMMAND_ROOT_BEGIN(libcsp, ROOT_HELP)
COMMAND_ROOT_ADD(libcsp, ping, cmd_ping, "<node>", "Pings a specific CSP Node")
COMMAND_ROOT_ADD(libcsp, ident, cmd_ident, "<node>", "Identifies a specific CSP Node")
COMMAND_ROOT_ADD(libcsp, debug, cmd_debug, "<level>", "Changes the debug logging level")
COMMAND_ROOT_ADD(libcsp, ps, cmd_ps, "<node> <timeout_ms>", "Runs ps on the remote node")
COMMAND_ROOT_ADD(libcsp, uptime, cmd_uptime, "<node>", "Gets node's CSP service uptime")
COMMAND_ROOT_END(libcsp, ROOT_HELP)

