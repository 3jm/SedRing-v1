# Current Issues
- ✔️ None, works & builds as intended.

# about cheat
- panic key (uninject): End
- show menu (future): Insert

# compiling
- set to Release
- build x86
- configuration type: Dynamic Library (.dll)
- Use Multi-Byte Character Set
- Linker-> SubSystem: Windows (/SUBSYSTEM:WINDOWS)

# images of cheat
- chams + box esp

![1](https://media.discordapp.net/attachments/843956305189535775/975563417547046982/unknown.png)

- Glow + ESP + Chicken esp 😏
![2](https://images-ext-2.discordapp.net/external/xofFU4iIlkWoZm2j_0NE67O0loFMuiJaC5136ZZ1UMw/%3Fwidth%3D1104%26height%3D621/https/media.discordapp.net/attachments/975764955246788688/975982358375305256/unknown.png)

# things to note
- chams are disabled, if you want this to work with DrawModel/DrawModelExecute chams, inside of your DM/DME hook, check if interfaces::studioRender->IsForcedMaterialOverride() is true. If it is true, that means it is a glow model & you DO NOT want to apply chams to it - that will make them compatible.

# road map
- [ ] convert it all to a imgui menu and go from there
