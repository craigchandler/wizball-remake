---
title: "First Contact With The Ancient Machine"
date: 2026-02-25
tags: ["devlog", "linux", "allegro", "debugging", "audio"]
---

This was the "can this ancient thing even run on modern Linux?" day, and the answer turned out to be "yes, but only after a lot of persuasion." The first rescue pass got the recovered WizBall remake compiling, added sanitizer builds, cleaned out generated junk, hardened data and tilemap loading, added scripting diagnostics, updated the Linux audio side to newer FMOD APIs, and generally stopped the game from falling over at startup; a wonderfully specific frame-0 portal bug also died here, which is exactly the sort of sentence that makes old game code sound haunted.
