---
title: "Save And Continue No Longer Feels Painful"
date: 2026-03-20
tags: ["devlog", "handheld", "save-game", "performance", "portmaster"]
---

The save-and-continue work has had a very welcome second phase: it is now fast enough to feel reasonable on the low-powered handheld target rather than like an awkward proof of concept. That matters more than it probably would on a desktop build, because a feature designed around short play sessions immediately loses its charm if stopping to save feels like a long interruption.

I am deliberately not going into all the engineering detail here, but the broad story is simple enough. The problem was not really the size of the save file on its own, it was the amount of unnecessary work wrapped around it. Tidying up that hot path made a much bigger difference than trying to shave a few bytes off the file itself. The end result is that save and load now feel brisk enough that the whole idea of dipping in, suspending a run, and coming back later makes sense on device.

That leaves the save system in a much healthier place for a release: the feature is there, it behaves like a proper handheld convenience feature, and it no longer drags the whole experience down every time I use it.
