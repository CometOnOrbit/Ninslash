Import("configure.lua")
Import("other/sdl3/sdl3.lua")
Import("other/freetype/freetype.lua")
Import("other/glew/glew.lua")

--- Setup Config -------
config = NewConfig()
config:Add(OptCCompiler("compiler"))
config:Add(OptTestCompileC("stackprotector", "int main(){return 0;}", "-fstack-protector -fstack-protector-all"))
config:Add(OptTestCompileC("minmacosxsdk", "int main(){return 0;}", "-mmacosx-version-min=10.5 -isysroot /Developer/SDKs/MacOSX10.5.sdk"))
config:Add(OptTestCompileC("macosxppc", "int main(){return 0;}", "-arch ppc"))
config:Add(OptLibrary("zlib", "zlib.h", false))
config:Add(SDL3.OptFind("sdl3", true))
config:Add(FreeType.OptFind("freetype", true))
config:Add(GLEW.OptFind("glew", true))
config:Finalize("config.lua")

python_in_path = ExecuteSilent("python -V") == 0

-- data compiler
function Python(name)
	if family == "windows" then
		name = str_replace(name, "/", "\\")
		if not python_in_path then
			-- Python is usually registered for .py files in Windows
			return name
		end
	end
	return "python " .. name
end

function CHash(output, ...)
	local inputs = TableFlatten({...})

	output = Path(output)

	-- compile all the files
	local cmd = Python("scripts/cmd5.py") .. " "
	for index, inname in ipairs(inputs) do
		cmd = cmd .. Path(inname) .. " "
	end

	cmd = cmd .. " > " .. output

	AddJob(output, "cmd5 " .. output, cmd)
	for index, inname in ipairs(inputs) do
		AddDependency(output, inname)
	end
	AddDependency(output, "scripts/cmd5.py")
	return output
end

--[[
function DuplicateDirectoryStructure(orgpath, srcpath, dstpath)
	for _,v in pairs(CollectDirs(srcpath .. "/")) do
		MakeDirectory(dstpath .. "/" .. string.sub(v, string.len(orgpath)+2))
		DuplicateDirectoryStructure(orgpath, v, dstpath)
	end
end

DuplicateDirectoryStructure("src", "src", "objs")
]]

function ResCompile(scriptfile)
	scriptfile = Path(scriptfile)
	if config.compiler.driver == "cl" then
		output = PathBase(scriptfile) .. ".res"
		AddJob(output, "rc " .. scriptfile, "rc /fo " .. output .. " " .. scriptfile)
	elseif config.compiler.driver == "gcc" then
		output = PathBase(scriptfile) .. ".coff"
		AddJob(output, "windres " .. scriptfile, "windres -i " .. scriptfile .. " -o " .. output)
	end
	AddDependency(output, scriptfile)
	return output
end

function Dat2c(datafile, sourcefile, arrayname)
	datafile = Path(datafile)
	sourcefile = Path(sourcefile)

	AddJob(
		sourcefile,
		"dat2c " .. PathFilename(sourcefile) .. " = " .. PathFilename(datafile),
		Python("scripts/dat2c.py").. "\" " .. sourcefile .. " " .. datafile .. " " .. arrayname
	)
	AddDependency(sourcefile, datafile)
	return sourcefile
end

function ContentCompile(action, output)
	output = Path(output)
	AddJob(
		output,
		action .. " > " .. output,
		--Python("datasrc/compile.py") .. "\" ".. Path(output) .. " " .. action
		Python("datasrc/compile.py") .. " " .. action .. " > " .. Path(output)
	)
	AddDependency(output, Path("datasrc/content.py")) -- do this more proper
	AddDependency(output, Path("datasrc/network.py"))
	AddDependency(output, Path("datasrc/compile.py"))
	AddDependency(output, Path("datasrc/datatypes.py"))
	AddDependency(output, Path("datasrc/weapon_types.py"))
	return output
end

function EmbedBinary(input, output, symbol)
	input = Path(input)
	output = Path(output)
	AddJob(output, "embed " .. output, Python("scripts/embed_binary.py") .. " " .. input .. " " .. symbol .. " > " .. output)
	AddDependency(output, input)
	AddDependency(output, Path("scripts/embed_binary.py"))
	return output
end

function EmbedBinaries(inputs, output, symbol)
	output = Path(output)
	local command = Python("scripts/embed_binary.py")
	for _, input in ipairs(inputs) do
		command = command .. " " .. Path(input)
	end
	AddJob(output, "embed " .. output, command .. " " .. symbol .. " > " .. output)
	for _, input in ipairs(inputs) do
		AddDependency(output, Path(input))
	end
	AddDependency(output, Path("scripts/embed_binary.py"))
	return output
end

function ManifestFiles(manifest)
	local result = {}
	for line in io.lines(manifest) do
		if line ~= "" and string.sub(line, 1, 1) ~= "#" then
			table.insert(result, "data/weapons/" .. line)
		end
	end
	return result
end

function EmbedManifest(manifest, output, symbol)
	output = Path(output)
	AddJob(output, "embed " .. output, Python("scripts/embed_binary.py") .. " @" .. Path(manifest) .. " " .. symbol .. " > " .. output)
	AddDependency(output, Path(manifest))
	for _, input in ipairs(ManifestFiles(manifest)) do
		AddDependency(output, Path(input))
	end
	AddDependency(output, Path("scripts/embed_binary.py"))
	return output
end

-- Content Compile
network_source = ContentCompile("network_source", "src/generated/protocol.cpp")
network_header = ContentCompile("network_header", "src/generated/protocol.h")
game_content_source = ContentCompile("game_content_source", "src/generated/game_data.cpp")
game_content_header = ContentCompile("game_content_header", "src/generated/game_data.h")
weapon_dsl = EmbedBinary("data/weapons/weapon_dsl.lua", "src/generated/weapon_dsl.inc", "gs_aWeaponDslLua")
official_weapons = EmbedManifest("data/weapons/official_manifest.txt", "src/generated/official_weapons.inc", "gs_aOfficialWeaponsLua")

AddDependency(network_source, network_header)
AddDependency(game_content_source, game_content_header)

nethash = CHash("src/generated/nethash.cpp", "src/engine/shared/protocol.h", "src/generated/protocol.h", "src/game/tuning.h", "src/game/gamecore.cpp", network_header)

client_link_other = {}
client_depends = {}
server_link_other = {}

if family == "windows" then
	if platform == "win32" then
		table.insert(client_depends, CopyToDirectory(".", "other\\freetype\\windows\\lib32\\freetype.dll"))
		table.insert(client_depends, CopyToDirectory(".", "other\\sdl3\\windows\\lib32\\SDL3.dll"))
		table.insert(client_depends, CopyToDirectory(".", "other\\glew\\windows\\lib32\\glew32.dll"))
	else
		table.insert(client_depends, CopyToDirectory(".", "other\\freetype\\windows\\lib64\\freetype.dll"))
		table.insert(client_depends, CopyToDirectory(".", "other\\sdl3\\windows\\lib64\\SDL3.dll"))
		table.insert(client_depends, CopyToDirectory(".", "other\\glew\\windows\\lib64\\glew32.dll"))
	end

	if config.compiler.driver == "cl" then
		client_link_other = {ResCompile("other/icons/ninslash_cl.rc")}
		server_link_other = {ResCompile("other/icons/ninslash_srv_cl.rc")}
	elseif config.compiler.driver == "gcc" then
		client_link_other = {ResCompile("other/icons/ninslash_gcc.rc")}
		server_link_other = {ResCompile("other/icons/ninslash_srv_gcc.rc")}
	end
end

function CompilerCacheTag()
	local compiler = config.compiler.cxx_compiler
	if not compiler or compiler == false then
		if config.compiler.driver == "cl" then
			compiler = "cl"
		elseif config.compiler.driver == "clang" then
			compiler = "clang++"
		else
			compiler = "g++"
		end
	end

	local version_flag = "--version"
	if config.compiler.driver == "gcc" then
		version_flag = "-dumpfullversion -dumpversion"
	elseif config.compiler.driver == "cl" then
		version_flag = ""
	end
	MakeDirectory(".bam")
	local version_file = ".bam/compiler-version-" .. config.compiler.driver .. ".txt"
	Execute('"' .. compiler .. '" ' .. version_flag .. ' > "' .. version_file .. '" 2>&1')
	local version = "unknown"
	local file = io.open(version_file, "r")
	if file then
		local line = file:read("*l")
		file:close()
		if line and line ~= "" then
			version = line
		end
	end

	local identity = config.compiler.driver .. "_" .. version .. "_" .. compiler
	identity = string.gsub(identity, "[^%w%._%-]", "_")
	return string.sub(identity, 1, 96)
end

compiler_cache_tag = CompilerCacheTag()

function Intermediate_Output(settings, input)
	return "objs/" .. compiler_cache_tag .. "/" .. string.sub(PathBase(input), string.len("src/")+1) .. settings.config_ext
end

function build(settings)
	-- apply compiler settings
	config.compiler:Apply(settings)
	
	--settings.objdir = Path("objs")
	settings.cc.Output = Intermediate_Output

	if config.compiler.driver == "cl" then
		settings.cc.flags:Add("/wd4244")
		settings.cc.flags:Add("/wd4503") -- warning C4503: decorated name length exceeded, name was truncated
		settings.cc.flags:Add("/EHsc") -- warnings of std containers
	else
		settings.cc.flags:Add("-Wall", "-fno-exceptions", "-Werror=format")
		if family == "windows" then
			-- disable visibility attribute support for gcc on windows
			settings.cc.defines:Add("NO_VIZ")
		elseif platform == "macosx" then
			settings.cc.flags:Add("-mmacosx-version-min=10.5")
			settings.link.flags:Add("-mmacosx-version-min=10.5")
			if config.minmacosxsdk.value == 1 then
				settings.cc.flags:Add("-isysroot /Developer/SDKs/MacOSX10.5.sdk")
				settings.link.flags:Add("-isysroot /Developer/SDKs/MacOSX10.5.sdk")
			end
		elseif config.stackprotector.value == 1 then
			settings.cc.flags:Add("-fstack-protector", "-fstack-protector-all")
			settings.link.flags:Add("-fstack-protector", "-fstack-protector-all")
		end
	end

	-- set some platform specific settings
	settings.cc.includes:Add("src")
	settings.cc.includes:Add("other/lua/src")

	if family == "unix" then
		if platform == "macosx" then
			settings.link.frameworks:Add("CoreFoundation")
			settings.link.frameworks:Add("AppKit")
		else
			settings.link.libs:Add("pthread")
		end
		
		if platform == "solaris" then
		    settings.link.flags:Add("-lsocket")
		    settings.link.flags:Add("-lnsl")
		end
	elseif family == "windows" then
		settings.link.libs:Add("gdi32")
		settings.link.libs:Add("user32")
		settings.link.libs:Add("ws2_32")
		settings.link.libs:Add("ole32")
		settings.link.libs:Add("shell32")
		settings.link.libs:Add("comdlg32")
		settings.link.libs:Add("userenv")
		if config.compiler.driver == "gcc" then
			-- Match CMakeLists.txt: ship without MinGW runtime DLLs
			-- (libstdc++-6.dll / libgcc_s_seh-1.dll / libwinpthread-1.dll).
			settings.link.flags:Add("-static-libgcc", "-static-libstdc++")
			settings.link.flags:Add("-Wl,-Bstatic", "-lstdc++", "-lpthread", "-Wl,-Bdynamic")
		end
	end

	-- compile zlib if needed
	if config.zlib.value == 1 then
		settings.link.libs:Add("z")
		if config.zlib.include_path then
			settings.cc.includes:Add(config.zlib.include_path)
		end
		zlib = {}
	else
		zlib = Compile(settings, Collect("src/engine/external/zlib/*.c"))
		settings.cc.includes:Add("src/engine/external/zlib")
	end

	-- build the small libraries
	wavpack = Compile(settings, Collect("src/engine/external/wavpack/*.c"))
	pnglite = Compile(settings, Collect("src/engine/external/pnglite/*.c"))
	json_parser = Compile(settings, Collect("src/engine/external/json-parser/*.c"))
	lua_sources = Collect("other/lua/src/*.c")
	for index = #lua_sources, 1, -1 do
		local filename = PathFilename(lua_sources[index])
		if filename == "lua.c" or filename == "luac.c" or filename == "onelua.c" or filename == "liolib.c" or filename == "loslib.c" or filename == "loadlib.c" or filename == "ldblib.c" then
			table.remove(lua_sources, index)
		end
	end
	lua = StaticLibrary(settings, "ninslash_lua", Compile(settings, lua_sources))
	
	-- Keep the C++ standard aligned with CMake. Add it after compiling the C
	-- libraries so the flag is only applied to engine and game C++ sources.
	if config.compiler.driver == "gcc" or config.compiler.driver == "clang" then
		settings.cc.flags_cxx:Add("--std=c++17")
	elseif config.compiler.driver == "cl" then
		settings.cc.flags_cxx:Add("/std:c++17")
	end

	-- build game components
	engine_settings = settings:Copy()
	server_settings = engine_settings:Copy()
	client_settings = engine_settings:Copy()
	launcher_settings = engine_settings:Copy()

	if family == "unix" then
		if platform == "macosx" then
			client_settings.link.frameworks:Add("OpenGL")
			client_settings.link.frameworks:Add("Cocoa")
			client_settings.link.frameworks:Add("CoreFoundation")
			launcher_settings.link.frameworks:Add("Cocoa")
		else
			client_settings.link.libs:Add("X11")
			client_settings.link.libs:Add("GL")
			client_settings.link.libs:Add("GLU")
		end

	elseif family == "windows" then
		client_settings.link.libs:Add("opengl32")
		client_settings.link.libs:Add("glu32")
		client_settings.link.libs:Add("winmm")
	end

	-- apply sdl settings
	config.sdl3:Apply(client_settings)
	-- apply freetype settings
	config.freetype:Apply(client_settings)
	-- apply glew settings
	config.glew:Apply(client_settings)

	glew = {}
	if config.glew.value == true and config.glew.use_source == true then
		glew = Compile(client_settings, "other/glew/src/glew.c")
	end

	engine_settings.cc.defines:Add("NOUNCRYPT")
	engine = Compile(engine_settings, Collect("src/engine/shared/*.cpp", "src/base/*.c", "src/engine/external/minizip/*.c"))
	client = Compile(client_settings, Collect("src/engine/client/*.cpp"))
	server = Compile(server_settings, Collect("src/engine/server/*.cpp"))

	versionserver = Compile(settings, Collect("src/versionsrv/*.cpp"))
	masterserver = Compile(settings, Collect("src/mastersrv/*.cpp"))
	game_shared = Compile(settings, Collect("src/game/*.cpp"), nethash, network_source, game_content_source)
	for _, object in ipairs(game_shared) do
		AddDependency(object, official_weapons)
		AddDependency(object, weapon_dsl)
	end
	game_client = Compile(settings, CollectRecursive("src/game/client/*.cpp"))
	game_server = Compile(settings, CollectRecursive("src/game/server/*.cpp"))
	game_editor = Compile(settings, Collect("src/game/editor/*.cpp"))

	-- build tools (TODO: fix this so we don't get double _d_d stuff)
	tools_src = Collect("src/tools/*.cpp", "src/tools/*.c")

	server_osxlaunch = {}
	if platform == "macosx" then
		server_osxlaunch = Compile(launcher_settings, "src/osxlaunch/server.m")
	end

	tools = {}
	for i,v in ipairs(tools_src) do
		toolname = PathFilename(PathBase(v))
		tools[i] = Link(settings, toolname, Compile(settings, v), engine, zlib, pnglite, json_parser)
	end

	-- build client, server, version server and master server
	client_exe = Link(client_settings, "ninslash", game_shared, game_client,
		engine, client, game_editor, zlib, pnglite, wavpack, json_parser, glew,
		lua, client_link_other)

	server_exe = Link(server_settings, "ninslash_srv", engine, server,
		game_shared, game_server, zlib, server_link_other, json_parser, lua)

	serverlaunch = {}
	if platform == "macosx" then
		serverlaunch = Link(launcher_settings, "serverlaunch", server_osxlaunch)
	end

	versionserver_exe = Link(server_settings, "versionsrv", versionserver,
		engine, zlib, json_parser)

	masterserver_exe = Link(server_settings, "mastersrv", masterserver,
		engine, zlib, json_parser)

	-- make targets
	c = PseudoTarget("client".."_"..settings.config_name, client_exe, client_depends)
	s = PseudoTarget("server".."_"..settings.config_name, server_exe, serverlaunch)
	g = PseudoTarget("game".."_"..settings.config_name, client_exe, server_exe)

	v = PseudoTarget("versionserver".."_"..settings.config_name, versionserver_exe)
	m = PseudoTarget("masterserver".."_"..settings.config_name, masterserver_exe)
	t = PseudoTarget("tools".."_"..settings.config_name, tools)

	all = PseudoTarget(settings.config_name, c, s, v, m, t)
	return all
end


debug_settings = NewSettings()
debug_settings.config_name = "debug"
debug_settings.config_ext = "_d"
debug_settings.debug = 1
debug_settings.optimize = 0
debug_settings.cc.defines:Add("CONF_DEBUG")

release_settings = NewSettings()
release_settings.config_name = "release"
release_settings.config_ext = ""
release_settings.debug = 0
release_settings.optimize = 1
release_settings.cc.defines:Add("CONF_RELEASE")

if platform == "macosx" then
	debug_settings_ppc = debug_settings:Copy()
	debug_settings_ppc.config_name = "debug_ppc"
	debug_settings_ppc.config_ext = "_ppc_d"
	debug_settings_ppc.cc.flags:Add("-arch ppc")
	debug_settings_ppc.link.flags:Add("-arch ppc")
	debug_settings_ppc.cc.defines:Add("CONF_DEBUG")

	release_settings_ppc = release_settings:Copy()
	release_settings_ppc.config_name = "release_ppc"
	release_settings_ppc.config_ext = "_ppc"
	release_settings_ppc.cc.flags:Add("-arch ppc")
	release_settings_ppc.link.flags:Add("-arch ppc")
	release_settings_ppc.cc.defines:Add("CONF_RELEASE")

	ppc_d = build(debug_settings_ppc)
	ppc_r = build(release_settings_ppc)

	if arch == "ia32" or arch == "amd64" then
		debug_settings_x86 = debug_settings:Copy()
		debug_settings_x86.config_name = "debug_x86"
		debug_settings_x86.config_ext = "_x86_d"
		debug_settings_x86.cc.flags:Add("-arch i386")
		debug_settings_x86.link.flags:Add("-arch i386")
		debug_settings_x86.cc.defines:Add("CONF_DEBUG")

		release_settings_x86 = release_settings:Copy()
		release_settings_x86.config_name = "release_x86"
		release_settings_x86.config_ext = "_x86"
		release_settings_x86.cc.flags:Add("-arch i386")
		release_settings_x86.link.flags:Add("-arch i386")
		release_settings_x86.cc.defines:Add("CONF_RELEASE")
	
		x86_d = build(debug_settings_x86)
		x86_r = build(release_settings_x86)
	end

	if arch == "amd64" then
		debug_settings_x86_64 = debug_settings:Copy()
		debug_settings_x86_64.config_name = "debug_x86_64"
		debug_settings_x86_64.config_ext = "_x86_64_d"
		debug_settings_x86_64.cc.flags:Add("-arch x86_64")
		debug_settings_x86_64.link.flags:Add("-arch x86_64")
		debug_settings_x86_64.cc.defines:Add("CONF_DEBUG")

		release_settings_x86_64 = release_settings:Copy()
		release_settings_x86_64.config_name = "release_x86_64"
		release_settings_x86_64.config_ext = "_x86_64"
		release_settings_x86_64.cc.flags:Add("-arch x86_64")
		release_settings_x86_64.link.flags:Add("-arch x86_64")
		release_settings_x86_64.cc.defines:Add("CONF_RELEASE")

		x86_64_d = build(debug_settings_x86_64)
		x86_64_r = build(release_settings_x86_64)
	end

	DefaultTarget("game_debug_x86")
	
	if config.macosxppc.value == 1 then
		if arch == "ia32" then
			PseudoTarget("release", ppc_r, x86_r)
			PseudoTarget("debug", ppc_d, x86_d)
			PseudoTarget("server_release", "server_release_ppc", "server_release_x86")
			PseudoTarget("server_debug", "server_debug_ppc", "server_debug_x86")
			PseudoTarget("client_release", "client_release_ppc", "client_release_x86")
			PseudoTarget("client_debug", "client_debug_ppc", "client_debug_x86")
		elseif arch == "amd64" then
			PseudoTarget("release", ppc_r, x86_r, x86_64_r)
			PseudoTarget("debug", ppc_d, x86_d, x86_64_d)
			PseudoTarget("server_release", "server_release_ppc", "server_release_x86", "server_release_x86_64")
			PseudoTarget("server_debug", "server_debug_ppc", "server_debug_x86", "server_debug_x86_64")
			PseudoTarget("client_release", "client_release_ppc", "client_release_x86", "client_release_x86_64")
			PseudoTarget("client_debug", "client_debug_ppc", "client_debug_x86", "client_debug_x86_64")
		else
			PseudoTarget("release", ppc_r)
			PseudoTarget("debug", ppc_d)
			PseudoTarget("server_release", "server_release_ppc")
			PseudoTarget("server_debug", "server_debug_ppc")
			PseudoTarget("client_release", "client_release_ppc")
			PseudoTarget("client_debug", "client_debug_ppc")
		end
	else
		if arch == "ia32" then
			PseudoTarget("release", x86_r)
			PseudoTarget("debug", x86_d)
			PseudoTarget("server_release", "server_release_x86")
			PseudoTarget("server_debug", "server_debug_x86")
			PseudoTarget("client_release", "client_release_x86")
			PseudoTarget("client_debug", "client_debug_x86")
		elseif arch == "amd64" then
			PseudoTarget("release", x86_r, x86_64_r)
			PseudoTarget("debug", x86_d, x86_64_d)
			PseudoTarget("server_release", "server_release_x86", "server_release_x86_64")
			PseudoTarget("server_debug", "server_debug_x86", "server_debug_x86_64")
			PseudoTarget("client_release", "client_release_x86", "client_release_x86_64")
			PseudoTarget("client_debug", "client_debug_x86", "client_debug_x86_64")
		end
	end
else
	build(debug_settings)
	build(release_settings)
	DefaultTarget("game_debug")
end
