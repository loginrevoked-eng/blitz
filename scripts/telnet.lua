local telnet = require("probe.telnet")
local logging = require("logging")

local M = {}
local TCP_OPEN = 0
local candidate_ports = {
    [23] = true,
    [2323] = true,
}

function M.supports(port)
    return candidate_ports[port] == true
end

function M.main(host, port)
    local supported_probe
    for _, probe in ipairs(host.tcp or {}) do
        if probe.result == TCP_OPEN and M.supports(probe.port) and
            (port == nil or probe.port == port) then
            supported_probe = probe
            break
        end
    end
    if not supported_probe then return false end

    local result, reason = telnet.confirm(host.ip, supported_probe.port,
        host.timeout_ms)
    if result then
        logging.log(string.format("Telnet confirmed on %s:%d (%s option %d)",
            host.ip, supported_probe.port, result.command, result.option))
        return true
    end
    logging.warn(string.format("Telnet handshake failed on %s:%d: %s",
        host.ip, supported_probe.port, reason or "no response"))
    return false
end

return M
