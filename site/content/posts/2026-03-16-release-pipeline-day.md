---
title: "Shipping Things Properly"
date: 2026-03-16
tags: ["devlog", "ci", "github-actions", "portmaster", "tooling"]
---

Today was entirely infrastructure: no gameplay code touched, no rendering bugs fixed, but it felt like a necessary kind of progress. The project now has a real release pipeline. Push a tag, and GitHub Actions builds Linux, Windows, and a combined PortMaster zip for both aarch64 and armhf, all in parallel, and assembles a GitHub Release automatically. That is the kind of thing that is easy to put off indefinitely while you convince yourself it can wait until later. It cannot wait until later.

The PortMaster package was the most interesting piece to think through. PortMaster expects a zip named exactly `WizBall.zip` containing a launcher script at the top level and everything else inside a `wizball/` subdirectory. It also expects both architecture binaries to be present in the same zip rather than separate downloads — the installer picks the right one at deploy time. That meant the workflow could not just bolt a packaging step onto each build job. Instead the two arch builds each produce a bare binary artifact, and a separate assembly job pulls both together with the game data and all the metadata files from the repo before zipping. The result is one artifact that PortMaster can actually use.

The game data situation is its own small problem. The data files the game needs at runtime — tilesets, tilemaps, scripts, sprite sheets, sound effects, the lot — are not all checked in as-is. Some of them get regenerated from source by running the game binary itself with headless rebuild flags. All three modes (`-rebuildscripts`, `-rebuildtilesets`, `-rebuildtilemaps`) exit cleanly before SDL creates a window, which makes them safe to run in a headless CI environment. A dedicated pre-job builds a native Linux binary, runs all three, and then copies both the generated binaries and the static assets into an artifact that every downstream build job picks up. That keeps the game data assembly in one place rather than duplicated across four jobs. Next step is to make a new package file format to reduce the raw files i'm shipping
