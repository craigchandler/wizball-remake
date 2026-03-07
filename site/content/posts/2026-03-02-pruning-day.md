---
title: "Less Code, More Progress"
date: 2026-03-02
tags: ["devlog", "sdl2", "refactoring", "renderer", "allegro"]
---

Today was a reminder that sometimes progress looks like deleting code without setting the building on fire: unused texture and blending functions were cut, display scaling was simplified, and the SDL2 renderer path got less tangled up with leftovers from the Allegro/OpenGL era. This was also good coding-agent territory, because Codex and Claude via GitHub Copilot are quite happy to take first passes at pruning and simplification, provided you keep a close eye on them and make sure they have not removed some bizarre old ritual that was secretly keeping the whole game stable.
