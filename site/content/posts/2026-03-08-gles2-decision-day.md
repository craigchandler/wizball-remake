---
title: "Deciding To Target GLES2"
date: 2026-03-08
tags: ["devlog", "portmaster", "handheld", "gles2", "rendering"]
---

Today feels like the point where I stop treating GLES2 as an experiment and start treating it as the PortMaster target. The reasoning is fairly plain: there is already a real direct GLES2 path here rather than just SDL's backend doing the work for me, the device build wants that renderer to be the default path, and if I am going to get proper handheld performance it probably comes from pushing batching and draw ownership deeper into the renderer instead of staying close to the old immediate-style SDL flow.