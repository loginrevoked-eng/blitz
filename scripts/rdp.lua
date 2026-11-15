local rdp = require("probe.rdp")
local logging = require("logging")

local M = {}
local TCP_OPEN = 0
local RDP_PORT = 3389

function M.supports(port)
    return port == RDP_PORT
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
    if not supported_probe then
        logging.debug(string.format("RDP skipped on %s: TCP/%d was not open",
            host.ip, RDP_PORT))
        return false
    end

    local result, reason = rdp.confirm(host.ip, supported_probe.port,
        host.timeout_ms)
    if result then
        if result.negotiated then
            logging.log(string.format("RDP confirmed on %s:%d (security %s)",
                host.ip, supported_probe.port, result.selected_protocol_name))
        elseif result.failure_code then
            logging.warn(string.format("RDP endpoint on %s:%d rejected negotiation (code %d)",
                host.ip, supported_probe.port, result.failure_code))
        else
            logging.log(string.format("Legacy RDP confirmed on %s:%d",
                host.ip, supported_probe.port))
        end
        return true
    end
    logging.warn(string.format("RDP handshake failed on %s:%d: %s",
        host.ip, supported_probe.port, reason or "no response"))
    return false
end

return M
