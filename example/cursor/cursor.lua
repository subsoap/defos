local M = {}

M.node = nil
M.ready = false
M.waiting_to_enable = false
M.waiting_to_disable = false

function M.set_software_cursor_texture()
end

function M.set_software_cursor_enabled(flag)
	if flag == true then
		M.waiting_to_enable = true
	else
		M.waiting_to_disable = true
	end
end



function M.update()
	if M.waiting_to_enable == true then
		M.waiting_to_enable = false
		gui.set_enabled(M.node, true)
	end
	if M.waiting_to_disable == true then
		M.waiting_to_disable = false
		gui.set_enabled(M.node, false)
	end	
end


return M