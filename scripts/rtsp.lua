local rtsp = require("probe.rtsp")
local logging = require("logging")

local M = {}
local TCP_OPEN = 0
local candidate_ports = {
    [554] = true,
    [8554] = true,
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

    local result, reason = rtsp.confirm(host.ip, supported_probe.port,
        host.timeout_ms)
    if result then
        logging.log(string.format("RTSP %s confirmed on %s:%d (status %d)",
            result.version, host.ip, supported_probe.port, result.status_code))
        return true
    end
    logging.warn(string.format("RTSP handshake failed on %s:%d: %s",
        host.ip, supported_probe.port, reason or "no response"))
    return false
end

return M
