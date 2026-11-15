local logging = require("logging")

local handlers = {
    require("scripts.ssh"),
    require("scripts.telnet"),
    require("scripts.rtsp"),
    require("scripts.rdp"),
}

local M = {}
local TCP_OPEN = 0

function M.main(host)
    if type(host) ~= "table" then
        return false
    end

    for _, probe in ipairs(host.tcp or {}) do
        if probe.result == TCP_OPEN then
            for _, handler in ipairs(handlers) do
                if handler.supports(probe.port) then
                    local ok, confirmed = pcall(handler.main, host, probe.port)
                    if not ok then
                        logging.error("script handler failed:", confirmed)
                    elseif confirmed then
                        return true
                    end
                    break
                end
            end
        end
    end
    return false
end

return M
