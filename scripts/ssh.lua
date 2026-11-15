local ssh = require("probe.ssh")
local logging = require("logging")

local M = {}
local TCP_OPEN = 0
local SSH_PORT = 22

function M.supports(port)
    return port == SSH_PORT
end

function M.main(host, port)
    local supported_port = false
    for _, probe in ipairs(host.tcp or {}) do
        if probe.result == TCP_OPEN and M.supports(probe.port) and
            (port == nil or probe.port == port) then
            supported_port = true
            break
        end
    end
    if not supported_port then return false end

    local identification = host.ssh_identification or host.ssh_banner
    if type(identification) ~= "string" then
        return false
    end

    local ok, parsed = pcall(ssh.parse_identification, identification)
    if not ok then
        return false
    end

    logging.log(string.format("SSH %s %s", host.ip, parsed.software_version))
    return true
end

return M
