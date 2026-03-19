---
title: "First Release Into The World"
date: 2026-03-17
tags: ["devlog", "release", "portmaster", "handheld", "ci"]
---

Today was the first time this stopped being purely an internal project and became something I could actually hand to other people. I cut the first proper release, let the pipeline do its work, and then submitted the PortMaster package for testing. That is a very different feeling from getting a local build running on my own device. Local success still leaves plenty of room for invisible assumptions; handing it over for PortMaster testing is the point where those assumptions get interrogated by reality.

It also feels like a useful milestone because so much of the last stretch was invisible engineering: packaging layout, launcher behaviour, generated data, cross-arch builds, GitHub Actions, and all the little boring details that have to be correct before a handheld build is something more than a private experiment. The game is not done, obviously, but it has crossed an important line. There is now a release with a version number, a real downloadable package, and a submission sitting with PortMaster testers instead of just a pile of intent on my machine.
