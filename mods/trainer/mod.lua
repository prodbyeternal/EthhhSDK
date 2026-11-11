-- ===========================================================================
--  I-Ninja Trainer + live inspector (EthhhSDK Lua mod)
--
--  Everything here uses the public Lua API (game/memory/input/imgui/settings)
--  so it doubles as a reference for writing Lua mods.
-- ===========================================================================

local ui     = imgui
local ACCENT = ui.hex("6C8CFF")
local OK     = ui.hex("5FD38D")
local WARN   = ui.hex("FFB347")
local BAD    = ui.hex("FF6B6B")
local DIM    = ui.hex("8A93A6")

local F6, F7, F8 = 0x75, 0x76, 0x77

-- ---------------------------------------------------------------------------
--  Persisted state
-- ---------------------------------------------------------------------------
local S = {
    slowmo        = settings.get_bool("slowmo", false),
    timescale     = settings.get_float("timescale", 1.0),
    teleOffset    = settings.get_float("tele_offset", 0.0),
    statIndex     = tonumber(settings.get("stat_index", "120")) or 120,
    statValue     = tonumber(settings.get("stat_value", "1")) or 1,
    bitIndex      = tonumber(settings.get("bit_index", "120")) or 120,
    memAddr       = settings.get("mem_addr", "0051AB30"),
    memType       = settings.get("mem_type", "int"),
    inspectAddr   = tonumber(settings.get("inspect_addr", "0")) or 0,
}

local savedPos     = nil     -- {x,y,z} captured by the player actions
local lastPlayer   = nil     -- last known player address
local keyLatch     = {}
local teleportOff  = { x = 0.0, y = 0.0, z = 0.0 }

-- ---------------------------------------------------------------------------
--  Helpers
-- ---------------------------------------------------------------------------
local function fmtVec(x, y, z)
    return string.format("%.3f, %.3f, %.3f", x or 0, y or 0, z or 0)
end

local function hex(v)
    return string.format("0x%08X", v or 0)
end

local function playerInfo()
    local addr = game.player_addr()
    if not addr or addr == 0 then return nil end
    return game.read_strat(addr)
end

local function pressedOnce(vk)
    local down = input.key(vk)
    local was = keyLatch[vk] or false
    keyLatch[vk] = down
    return down and not was
end

local function checkboxSetting(label, key, def)
    local current = settings.get_bool(key, def)
    local value, changed = ui.checkbox(label, current)
    if changed then settings.set_bool(key, value) end
    return value, changed
end

local function distanceSq(a, b)
    local dx, dy, dz = a.x - b.x, a.y - b.y, a.z - b.z
    return dx * dx + dy * dy + dz * dz
end

local function applyTimescale()
    if input.overlay_open() then return end
    if S.slowmo then
        game.set_timescale(S.timescale)
    end
end

local function teleportPlayerToCamera(offset)
    local p = game.player_addr()
    local cam = game.camera()
    if not p or p == 0 or not cam then return false, "no player/camera" end
    offset = offset or S.teleOffset
    return game.write_strat_pos(p, cam.x + teleportOff.x,
                                   cam.y + teleportOff.y + offset,
                                   cam.z + teleportOff.z)
end

-- ---------------------------------------------------------------------------
--  Tick: keybinds + persistent trainer effects
-- ---------------------------------------------------------------------------
hook.add("tick", function(tickIndex, dt)
    applyTimescale()

    -- F6: teleport player to the camera position.
    if pressedOnce(F6) then
        local ok = teleportPlayerToCamera()
        log.info(ok and "trainer: player teleported to camera" or "trainer: teleport failed (no player)")
    end

    -- F7: save position, F8: restore position.
    if pressedOnce(F7) then
        local p = playerInfo()
        if p then
            savedPos = { x = p.x, y = p.y, z = p.z }
            log.info("trainer: saved position " .. fmtVec(p.x, p.y, p.z))
        end
    end
    if pressedOnce(F8) then
        local p = game.player_addr()
        if p and p ~= 0 and savedPos then
            game.write_strat_pos(p, savedPos.x, savedPos.y, savedPos.z)
            log.info("trainer: restored position")
        end
    end

    lastPlayer = game.player_addr()
end)

-- ---------------------------------------------------------------------------
--  GUI
-- ---------------------------------------------------------------------------
hook.add("gui", function()
    local cam = game.camera()
    local player = playerInfo()

    ------------------------------------------------------------- quick actions
    if ui.collapsing_header("Trainer", true) then
        ui.text_colored(DIM, "F6 teleport to camera   F7 save position   F8 restore position")

        if ui.button("Teleport player to camera", 240) then
            local ok, why = teleportPlayerToCamera()
            if not ok then log.warn("trainer: " .. tostring(why)) end
        end
        ui.same_line()
        if ui.button("Move camera to player", 240) and player then
            game.set_camera_pos(player.x, player.y, player.z)
        end

        ui.same_line()
        if ui.button("Save position", 130) and player then
            savedPos = { x = player.x, y = player.y, z = player.z }
        end
        ui.same_line()
        if ui.button("Restore position", 140) then
            local p = game.player_addr()
            if p and p ~= 0 and savedPos then
                game.write_strat_pos(p, savedPos.x, savedPos.y, savedPos.z)
            end
        end

        ui.spacing()
        ui.set_next_item_width(200)
        local offset = ui.drag_float("Teleport Y offset", S.teleOffset, 0.05, -20.0, 20.0)
        if offset ~= S.teleOffset then
            S.teleOffset = offset
            settings.set_float("tele_offset", offset)
        end
        ui.same_line()
        ui.text_disabled("applied on F6 / the teleport button")

        ui.separator()

        ------------------------------------------------------------ slow motion
        local slow, slowChanged = checkboxSetting("Slow motion", "slowmo", false)
        S.slowmo = slow
        if slowChanged and not slow then game.set_timescale(1.0) end
        ui.same_line()
        ui.set_next_item_width(240)
        local ts = ui.slider_float("Time scale", S.timescale, 0.05, 3.0, "%.2fx")
        if ts ~= S.timescale then
            S.timescale = ts
            settings.set_float("timescale", ts)
        end
        ui.same_line()
        if ui.button("1.0x##reset") then
            S.timescale = 1.0
            settings.set_float("timescale", 1.0)
            game.set_timescale(1.0)
        end

        ui.separator()

        ------------------------------------------------------------ debug flags
        ui.text_colored(ACCENT, "DEBUG FLAGS")
        local flags = game.debug_flags()
        local flagDefs = {
            { bit = game.FLAG_LEVEL_SELECT,      name = "Level select" },
            { bit = game.FLAG_DEMO_LEVEL_SELECT, name = "Demo level select" },
            { bit = game.FLAG_SOUND,             name = "Sound" },
            { bit = game.FLAG_DEBUG_CHEATS,      name = "Cheats" },
            { bit = game.FLAG_DEBUG_TEXT,        name = "Debug text" },
            { bit = game.FLAG_DEBUG_FLY,         name = "Fly" },
            { bit = game.FLAG_DEMO_BUILD,        name = "Demo build" },
            { bit = game.FLAG_DEMO_MAGAZINE,     name = "Demo magazine" },
        }
        for i, def in ipairs(flagDefs) do
            local on = math.floor(flags / def.bit) % 2 == 1
            local v, changed = ui.checkbox(def.name .. "##flag" .. i, on)
            if changed then game.toggle_flag(def.bit, v) end
            if i % 4 ~= 0 then ui.same_line() end
        end
        ui.text_disabled(string.format("debug flags = 0x%04X", flags))
    end

    ---------------------------------------------------------- player tracking
    if ui.collapsing_header("Player", true) then
        if player then
            ui.text_colored(OK, "TRACKED  " .. hex(player.addr))
            ui.same_line()
            ui.text_disabled("mode: " .. game.player_mode() .. "   class: " .. (player.class_name or hex(player.class)))
            ui.text("position  " .. fmtVec(player.x, player.y, player.z))
            if cam then
                local d = math.sqrt(distanceSq(player, { x = cam.x, y = cam.y, z = cam.z }))
                ui.same_line()
                ui.text_disabled(string.format("   %.2f units from camera", d))
            end
        else
            ui.text_colored(WARN, "No player detected yet - load a level or pick a strat below.")
        end
        if game.player_detection_info then ui.text(game.player_detection_info()) end

        if ui.button("Auto+") then game.set_player_mode("auto+") end
        ui.same_line()
        if ui.button("Auto") then game.set_player_mode("auto") end
        ui.same_line()
        if ui.button("Learn candidates (idle / move / stop)") then game.learn_player() end
        ui.text_disabled("Close F9: idle 1s, move with WASD/arrows 3s, stop 2s. Then confirm a candidate; learning never pins automatically.")
        ui.same_line()
        local mode = game.player_mode()
        ui.text_disabled("current mode: " .. mode)

        local manualText = ui.input_text("Strat address", string.format("%08X", S.inspectAddr or 0), 16)
        local manual = tonumber(manualText, 16)
        if manual and manual ~= S.inspectAddr then
            S.inspectAddr = math.floor(manual)
        end
        ui.same_line()
        if ui.button("Track this strat") then
            game.set_player_mode("manual")
            game.select_player(S.inspectAddr)
        end

        local cands = game.player_candidates(12)
        if cands and #cands > 0 then
            ui.spacing()
            ui.text_colored(ACCENT, "CANDIDATES (exact Ninja first; learning never auto-pins)")
            if ui.begin_table("##candidates", 5) then
                ui.table_setup_column("Address")
                ui.table_setup_column("Class")
                ui.table_setup_column("Score / dist")
                ui.table_setup_column("Position")
                ui.table_setup_column("")
                ui.table_headers_row()
                for _, c in ipairs(cands) do
                    ui.table_next_row()
                    ui.table_next_column()
                    ui.text(hex(c.addr))
                    ui.table_next_column()
                    ui.text(c.class_name or hex(c.class))
                    ui.table_next_column()
                    ui.text(string.format("%.2f / %.2f", c.learn_score or 0, c.camera_distance or 0))
                    ui.table_next_column()
                    ui.text(fmtVec(c.x, c.y, c.z))
                    ui.table_next_column()
                    ui.push_id(c.addr)
                    if ui.small_button("select") then
                        game.set_player_mode("manual")
                        game.select_player(c.addr)
                    end
                    ui.pop_id()
                end
                ui.end_table()
            end
        end
    end

    ------------------------------------------------------------- live state
    if ui.collapsing_header("Live state", true) then
        if ui.begin_table("##state", 2) then
            ui.table_setup_column("Key")
            ui.table_setup_column("Value")
            local rows = {
                { "Tick / frame", string.format("%d / %d", game.tick(), game.frame()) },
                { "Game time", string.format("%.3f s", game.time()) },
                { "Time step", string.format("%.5f s", game.time_step()) },
                { "Time scale", string.format("%.3f", game.timescale()) },
                { "Strats", tostring(game.strat_count()) },
                { "Camera", cam and fmtVec(cam.x, cam.y, cam.z) or "unavailable" },
                { "Camera FOV", cam and string.format("%.2f deg (%.4f rad)", cam.fov * 180.0 / math.pi, cam.fov) or "-" },
                { "Param104", cam and string.format("%.3f", cam.param104) or "-" },
                { "Device / HWND", hex(game.device()) .. " / " .. hex(game.hwnd()) },
            }
            for _, row in ipairs(rows) do
                ui.table_next_row()
                ui.table_next_column(); ui.text_disabled(row[1])
                ui.table_next_column(); ui.text(row[2])
            end
            ui.end_table()
        end
    end

    --------------------------------------------------------- nearby objects
    if ui.collapsing_header("Nearby strats", false) then
        if not cam then
            ui.text_disabled("Camera is not available yet.")
        else
            local limit = tonumber(settings.get("near_limit", "16")) or 16
            ui.set_next_item_width(120)
            local newLimit, changed = ui.slider_int("Limit", limit, 4, 64)
            if changed then settings.set("near_limit", tostring(newLimit)) end

            local all = game.strats(4096)
            local list = {}
            for _, s in ipairs(all) do
                if s.camera_distance and s.camera_distance >= 0 then
                    list[#list + 1] = s
                end
            end
            table.sort(list, function(a, b) return a.camera_distance < b.camera_distance end)

            if ui.begin_table("##near", 5) then
                ui.table_setup_column("Address")
                ui.table_setup_column("Dist")
                ui.table_setup_column("Position")
                ui.table_setup_column("Class")
                ui.table_setup_column("")
                ui.table_headers_row()
                for i = 1, math.min(#list, newLimit) do
                    local s = list[i]
                    ui.table_next_row()
                    ui.table_next_column(); ui.text(hex(s.addr))
                    ui.table_next_column(); ui.text(string.format("%.1f", s.camera_distance))
                    ui.table_next_column(); ui.text(fmtVec(s.x, s.y, s.z))
                    ui.table_next_column(); ui.text(s.class_name or hex(s.class))
                    ui.table_next_column()
                    ui.push_id(s.addr)
                    if ui.small_button("inspect") then
                        S.inspectAddr = s.addr
                        settings.set("inspect_addr", string.format("%d", s.addr))
                    end
                    ui.pop_id()
                end
                ui.end_table()
            end
        end
    end

    ------------------------------------------------------------ strat inspector
    if ui.collapsing_header("Strat inspector", false) then
        ui.set_next_item_width(150)
        local text = ui.input_text("Address (hex)", string.format("%08X", S.inspectAddr or 0), 16)
        ui.same_line()
        if ui.button("Read") then
            local v = tonumber(text, 16)
            if v then
                S.inspectAddr = v
                settings.set("inspect_addr", string.format("%d", v))
            end
        end

        local s = S.inspectAddr and S.inspectAddr ~= 0 and game.read_strat(S.inspectAddr) or nil
        if s then
            ui.text("addr  " .. hex(s.addr) .. "   class " .. hex(s.class))
            if s.class_name then ui.text("name  " .. s.class_name .. "   self handle " .. hex(s.self_handle)) end
            local identity = game.strat_identity and game.strat_identity(s.addr) or nil
            if identity then
                ui.text_disabled("model " .. hex(identity.model) .. "   animation " .. hex(identity.animation) .. "   nodes " .. hex(identity.node_matrices))
                if identity.owner_field_known then
                    ui.text_disabled("owner handle " .. hex(identity.raw_owner_handle) .. " -> " .. hex(identity.owner_addr) .. " (read-only, unresolved = 0)")
                end
            end
            ui.text("flags " .. hex(s.flags) .. "   type mask " .. hex(s.type_mask))
            ui.text("pos   " .. fmtVec(s.x, s.y, s.z) .. "   w " .. string.format("%.3f", s.w))
            ui.text(string.format("quat  %.3f, %.3f, %.3f, %.3f", s.qx, s.qy, s.qz, s.qw))

            local nx, cx = ui.drag_float("X##px", s.x, 0.05)
            local ny, cy = ui.drag_float("Y##py", s.y, 0.05)
            local nz, cz = ui.drag_float("Z##pz", s.z, 0.05)
            if cx or cy or cz then
                game.write_strat_pos(s.addr, nx, ny, nz)
            end
            if ui.button("Copy player position here") and player then
                game.write_strat_pos(s.addr, player.x, player.y, player.z)
            end
            ui.same_line()
            if ui.button("Make camera look target") and cam then
                game.set_camera_pos(s.x, s.y, s.z)
            end
        elseif S.inspectAddr and S.inspectAddr ~= 0 then
            ui.text_colored(WARN, "Strat is not readable (it may have been removed).")
        else
            ui.text_disabled("Pick a strat from the list above or enter an address.")
        end
    end

    --------------------------------------------------------------- save stats
    if ui.collapsing_header("Save stats", false) then
        ui.text_disabled("Index maps to the profile int table (0x34 + index*4) and bitfield (+0x1034).")
        ui.set_next_item_width(120)
        local idx, idxChanged = ui.input_int("Integer index", S.statIndex)
        if idxChanged then S.statIndex = idx; settings.set("stat_index", tostring(idx)) end
        ui.set_next_item_width(160)
        local val, valChanged = ui.input_int("Value", S.statValue)
        if valChanged then S.statValue = val; settings.set("stat_value", tostring(val)) end
        if ui.button("Read int") then
            local v = game.stat_int(S.statIndex)
            if v == nil then log.warn("trainer: stat index unreadable") else S.statValue = v end
        end
        ui.same_line()
        if ui.button("Write int") then
            if game.set_stat_int(S.statIndex, S.statValue) then
                log.info("trainer: wrote stat int")
            else
                log.warn("trainer: stat write failed")
            end
        end

        ui.spacing()
        ui.set_next_item_width(120)
        local bidx, bitChanged = ui.input_int("Bit index", S.bitIndex)
        if bitChanged then S.bitIndex = bidx; settings.set("bit_index", tostring(bidx)) end
        local currentBit = game.stat_bit(S.bitIndex)
        ui.text("current: " .. (currentBit == nil and "unreadable" or tostring(currentBit)))
        ui.same_line()
        if ui.button("Set true") then game.set_stat_bit(S.bitIndex, true) end
        ui.same_line()
        if ui.button("Set false") then game.set_stat_bit(S.bitIndex, false) end
    end

    ---------------------------------------------------------------- memory peek
    if ui.collapsing_header("Memory peek", false) then
        ui.set_next_item_width(140)
        S.memAddr = ui.input_text("Address", S.memAddr, 32)
        ui.same_line()
        local types = { "byte", "int16", "int", "float", "double", "string" }
        local typeIndex = 3
        for i, t in ipairs(types) do if t == S.memType then typeIndex = i end end
        ui.set_next_item_width(110)
        local newIndex, typeChanged = ui.combo("Type", typeIndex, types)
        if typeChanged then S.memType = types[newIndex]; settings.set("mem_type", S.memType) end

        if typeChanged or ui.button("Read##mem") then
            local addr = tonumber(S.memAddr, 16)
            if not addr then
                ui.text_colored(BAD, "Invalid address")
            else
                local value = nil
                if S.memType == "byte" then value = memory.read_byte(addr)
                elseif S.memType == "int16" then value = memory.read_int(addr) % 0x10000
                elseif S.memType == "int" then value = memory.read_int(addr)
                elseif S.memType == "float" then value = memory.read_float(addr)
                elseif S.memType == "double" then
                    local lo = memory.read_int(addr)
                    local hi = memory.read_int(addr + 4)
                    if lo and hi then
                        local sign = (hi >= 0) and 1 or -1
                        -- loose double reconstruction is not exposed; show raw halves
                        value = string.format("lo=%d hi=%d", lo, hi)
                    end
                elseif S.memType == "string" then value = memory.read_string(addr, 64) end
                if value == nil then
                    ui.text_colored(BAD, "Unreadable")
                else
                    ui.text(string.format("0x%08X = %s", addr, tostring(value)))
                end
            end
        end

        ui.spacing()
        ui.text_disabled("Write helpers (careful: these write directly into the process).")
        local writeAddr = tonumber(S.memAddr, 16) or 0
        ui.set_next_item_width(160)
        local wv, wc = ui.input_float("Float value", 0.0)
        if ui.button("Write float") then
            if memory.write_float(writeAddr, wv) then log.info("trainer: float written") else log.warn("trainer: write failed") end
        end
        ui.same_line()
        local wi, wic = ui.input_int("Int value", 0)
        if ui.button("Write int") then
            if memory.write_int(writeAddr, wi) then log.info("trainer: int written") else log.warn("trainer: write failed") end
        end
    end
end)

hook.add("overlay_open", function()
    -- The SDK owns overlay pause; do not overwrite its zero time scale.
end)

hook.add("overlay_close", function()
    applyTimescale()
end)

log.info("trainer: loaded (F6 teleport, F7 save, F8 restore)")
