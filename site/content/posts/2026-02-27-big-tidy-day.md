---
title: "The Great Untangling"
date: 2026-02-27
tags: ["devlog", "sdl2", "refactoring", "renderer", "input"]
---

This was a refactoring day rather than a flashy one: the new SDL2 platform, input, and renderer code got reorganised into something more readable and sustainable, which matters because messy migration code turns every future fix into archaeology. By now Codex and Claude via GitHub Copilot were doing most of the heavy lifting, and the real lesson for me was learning to manage coding agents well, because asking them to help is easy and stopping them from enthusiastically "improving" a load-bearing wall is the actual skill.
