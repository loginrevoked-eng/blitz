local sockets = require("sockets")
local logging = require("logging")

local M = {}
local TCP_OPEN = 0
local TIMEOUT = 5 -- seconds

function M.supports(_port)
    return true
end

local function ReadBanner(host, port)
    local socket, err = sockets.new()
    if not socket then
        logging.warn("banner socket create failed:", err)
        return false
    end

    local timeout_ms = TIMEOUT * 1000
    local ok
    ok, err = sockets.set_timeout(socket, timeout_ms)
    if not ok then
        logging.warn("banner timeout setup failed:", err)
        sockets.close(socket)
        return false
    end

    ok, err = sockets.connect(socket, host.ip, port)
    if not ok then
        logging.debug("banner connect failed on", host.ip, port, err)
        sockets.close(socket)
        return false
    end

    local deadline = os.time() + TIMEOUT
    local chunks = {}
    while true do
        local remaining_ms = (deadline - os.time()) * 1000
        if remaining_ms <= 0 then break end

        ok, err = sockets.set_timeout(socket, remaining_ms)
        if not ok then break end

        local bytes
        bytes, err = sockets.recv(socket, 4096)
        if not bytes then
            if not string.find(err or "", "10060", 1, true) then
                logging.debug("banner receive stopped on", host.ip, port, err)
            end
            break
        end
        if #bytes == 0 then break end
        chunks[#chunks + 1] = bytes
    end

    sockets.close(socket)

    local banner = table.concat(chunks)
    if #banner == 0 then
        logging.debug("no banner from", host.ip, port)
        return false
    end

    logging.log(string.format("banner from %s:%d: %q", host.ip, port, banner))
    return true
end

function M.main(host, port)
    if port ~= nil then
        return ReadBanner(host, port)
    end

    local found_banner = false
    for _, probe in ipairs(host.tcp or {}) do
        if probe.result == TCP_OPEN then
            found_banner = ReadBanner(host, probe.port) or found_banner
        end
    end
    return found_banner
end

return M
