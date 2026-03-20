# Creating an AI Documentation Style Guide for Classic Amiga Utility Readmes

## Executive summary

AI-written technical documentation tends to “give itself away” through a combination of (a) overly uniform tone and structure, (b) repetitive phrasing and transition-heavy sentence glue, and (c) generic, abstract nouns and benefit language that sounds more like modern product copy than a tool author talking to peers. Research syntheses and stylometric studies consistently point to AI text being more formal/impersonal, more repetitive, and more stylistically uniform than human writing. citeturn0search6turn9view6turn9view5turn9view4

Classic Amiga utility documentation (especially Aminet-style readmes) is typically plain text, compact, and operational: it starts with structured metadata fields; it states requirements and limitations bluntly; it uses short sections like “Installation”, “Usage”, “Known problems”, and “History”; and it leans on concrete details (Kickstart versions, memory figures, drawers, assigns, CLI templates) rather than “benefits”. It often includes candid caveats (“may not work properly”, “not tested on real hardware”, “this is a hack”), and it ends without marketing wrap-up—often just a contact line or a short sign-off. citeturn4view0turn11view2turn11view1turn10view3turn10view4turn2view4turn2view5turn7view0turn3view3

The most important operational rules to reproduce the latter while avoiding the former are:

Write like a competent utility author addressing competent users: state what it does, what it needs, how to install it, how to run it, what breaks, and what changed—without pep, recaps, or “seamless” language. citeturn11view2turn11view5turn2view5turn10view4

Use the Aminet readme conventions when targeting Aminet: include the machine-readable header fields; keep “Short:” short and non-boastful; then move quickly to practical documentation. citeturn4view0

Be explicitly honest about uncertainty and testing scope. Besides matching the Amiga corpus, this also reduces a core failure mode of language models: producing plausible-sounding statements that are wrong or unjustified. citeturn9view8turn10view4turn7view3turn3view3

## Method and evidence base

This framework is grounded in a small, representative corpus drawn from Aminet pages and readmes across multiple utility categories, selected to cover CLI tools, Workbench utilities, boot/system utilities, and packages with installer/usage notes. The corpus includes examples that show: (1) the Aminet readme field format and constraints; (2) classic sectioning and ASCII formatting; (3) requirements/compatibility patterns; (4) “known problems” disclosure style; and (5) version/history conventions. citeturn4view0turn11view2turn11view0turn10view2turn10view3turn10view4turn11view3turn11view5turn7view0

Key reference points for “Aminet-native” conventions are the Aminet readme format rules (machine-readable header “fields”, “Short:” length constraints, and guidance to avoid boasting or excessive uppercase). citeturn4view0

Key reference points for “classic utility voice” are readmes where authors explicitly state motivations (“I wrote it because…”), technical assumptions, and limitations in a direct tone; for example, classic CLI utilities explain why they exist and then immediately provide templates and argument semantics. citeturn11view2turn11view0turn11view5

Key reference points for “honest limitations” are packages that explicitly warn about hackiness, incomplete features, untested builds, or inability to reproduce bugs—patterns common in archive documentation and strongly at odds with generic AI “confidence”. citeturn3view3turn7view3turn10view4turn10view3

For the “AI-sounding” side, the anti-patterns are derived from empirically reported linguistic tendencies (formality/impersonality, lower lexical diversity, repetition), stylometry findings on uniformity, and practitioner-oriented writing analyses noting transition overuse and stock “AI phrases”. citeturn0search6turn9view6turn9view4turn9view5

## Anti-pattern catalogue for AI-sounding documentation

The catalogue below is written as an “avoid → replace” reference you can use both as generation constraints and as a rewrite checklist. Where noted, these patterns align with reported AI tendencies toward formality, repetition, and uniformity. citeturn0search6turn9view6turn9view4turn9view5

**Anti-pattern: Stock transition glue (“Furthermore”, “In addition”, “It’s important to note”).**  
What it is: Using too many explicit transitions and summarising link phrases to force cohesion.  
Why it reads AI: Overuse of “cohesive devices” makes prose feel formulaic and textbook-like; it also produces repetitive rhythm. citeturn9view4turn9view5  
How it appears: Paragraphs that start with “First… Second… Finally…” even when unnecessary; frequent “Additionally,” “Moreover,” “Therefore,” “In conclusion,”.  
Bad example: “Furthermore, it’s important to note that the tool is easy to use.”  
Amiga-appropriate rewrite: “Usage is straightforward. See the examples below.”  
Rewrite rule: Delete most transition words; rely on section breaks, blank lines, and concrete headings instead.

**Anti-pattern: Generic mission statements (“designed to”, “aims to”, “provides a solution”).**  
What it is: Abstract framing instead of describing what it does.  
Why it reads AI: It inflates without informing and tends toward impersonal corporate phrasing (a known AI tell). citeturn0search6turn9view5  
How it appears: “This utility is designed to streamline your workflow…”  
Bad example: “This utility is designed to enhance productivity.”  
Amiga-appropriate rewrite: “Copies icon images from one .info to another.” (Then state constraints.) citeturn11view2  
Rewrite rule: Replace “designed to” with an active verb plus object (“Converts…”, “Patches…”, “Monitors…”).

**Anti-pattern: Benefit language and inflated adjectives (“robust”, “seamless”, “powerful”, “cutting-edge”).**  
What it is: Marketing-adjacent claims without corresponding specifics.  
Why it reads AI: It sounds like modern product copy and is often paired with vague nouns.  
How it appears: “Powerful features”, “seamless integration”, “robust performance”.  
Bad example: “A powerful, robust tool that seamlessly integrates…”  
Amiga-appropriate rewrite: “A small CLI tool. Requires OS 2.0+. Does not run from Workbench.” citeturn11view2  
Rewrite rule: If an adjective doesn’t change a user action or constraint, remove it.

**Anti-pattern: Over-balanced, over-symmetric structure (school-essay cadence).**  
What it is: Predictable paragraph sizes, mirrored sentences, and uniform section lengths.  
Why it reads AI: Stylometry work suggests AI prose often follows a narrow, uniform pattern compared with human range. citeturn9view6turn0search6  
How it appears: Every section has three bullet points; every paragraph ends with a mini-summary.  
Bad example: “This section covered X. In the next section, we will cover Y.”  
Amiga-appropriate rewrite: Omit the recap. Add the actual next heading.

**Anti-pattern: Unnecessary recap / “In conclusion” endings.**  
What it is: A final paragraph that restates what was already stated.  
Why it reads AI: Feels padded and blog-like; classic readmes often end after the last operational section, sometimes with contact details. citeturn2view4turn2view5turn11view4  
Bad example: “In conclusion, this tool offers many benefits…”  
Amiga-appropriate rewrite: End with “History:” / “Contact:” / “Bugs:” as needed, then stop. citeturn11view4turn2view5turn10view4

**Anti-pattern: Empty friendliness (“Happy coding!”, “We’re excited to…”).**  
What it is: Cheerleading tone not anchored in technical content.  
Why it reads AI: It signals modern community/marketing norms; classic utility docs are practical first.  
Bad example: “We’re thrilled to introduce…”  
Amiga-appropriate rewrite: “New in 1.1: fixes X.” citeturn11view5turn11view3

**Anti-pattern: Polite smoothing that hides risk (“Please make sure to…”).**  
What it is: Indirect wording for warnings and constraints.  
Why it reads AI: Avoids the bluntness seen in classic readmes; also increases word count without increasing clarity.  
Bad example: “Please make sure to back up your files before proceeding.”  
Amiga-appropriate rewrite: “WARNING: This patches C:Copy. A backup is saved as C:Copy.old.” citeturn11view5

**Anti-pattern: Over-explaining obvious steps.**  
What it is: Turning a simple instruction into a tutorial paragraph.  
Why it reads AI: Padding; mismatch with audience assumptions in classic archives (users are expected to know how to unpack an archive, run Installer, copy to C:). citeturn11view5turn10view5turn11view4  
Bad example: “To install, first download the archive, then locate it, then extract it…”  
Amiga-appropriate rewrite: “Unpack to RAM:. Run Installer. Done.” citeturn11view5

**Anti-pattern: Vague nouns (“functionality”, “capabilities”, “solution”, “ecosystem”).**  
What it is: Abstract nouns standing in for concrete actions, files, commands, and limits.  
Why it reads AI: Common in generic AI prose (impersonal, noun-heavy patterns). citeturn0search6  
Bad example: “Provides functionality for icon management.”  
Amiga-appropriate rewrite: “Copies icon images, can convert formats, can snapshot positions.” citeturn10view5

**Anti-pattern: Fake certainty and missing test scope.**  
What it is: Declaring compatibility/performance as fact without test notes or bounds.  
Why it reads AI: LLMs are known to generate plausible but false statements confidently; an Amiga author often states what they tested and what they did not. citeturn9view8turn7view3turn10view4  
Bad example: “Works on all configurations and emulators.”  
Amiga-appropriate rewrite: “Not sure about emulators.” / “Not tested on real 68k.” / “Should work on OS 3.2; tested on X.” citeturn11view1turn7view3turn11view3

**Anti-pattern: Over-sanitised “no negatives” tone.**  
What it is: Presenting only features, never drawbacks.  
Why it reads AI: Classic readmes frequently include “Known problems” / “Limitations/Disclaimer” sections and candid warnings. citeturn10view3turn10view4turn7view0turn7view3  
Bad example: “No known issues at this time.”  
Amiga-appropriate rewrite: “Known problems: …” or “Limitations: …” or “Untested: …” with specifics. citeturn10view4turn7view3turn10view3

**Anti-pattern: Modern SaaS-doc conventions in a plain-text context.**  
What it is: Corporate headings like “Getting Started”, “FAQ”, “Best Practices”, heavy Markdown, or UX copy.  
Why it reads AI: It imports a different documentation culture; classic readmes commonly use simple headings with underlines or ASCII banners and get to the point. citeturn11view2turn11view1turn10view5turn7view0  
Bad example: “Getting started: simply click…”  
Amiga-appropriate rewrite: “Installation:” / “Usage:” / “Tooltypes:” (then details). citeturn11view4turn10view5turn7view0

**Anti-pattern: Over-regularised list grammar.**  
What it is: Perfect parallel bullet lists with identical phrasing.  
Why it reads AI: Contributes to the “uniform pattern” fingerprint. citeturn9view6  
Bad example: “It allows you to… It enables you to… It empowers you to…”  
Amiga-appropriate rewrite: Mix short fragments and full sentences where natural; keep bullets functional (options, files, limits). citeturn11view3turn11view4

**Anti-pattern: Telling the reader what the document will do (“This README will cover…”).**  
What it is: Meta commentary instead of content.  
Why it reads AI: Common default structuring in AI writing; classic readmes usually just *start*.  
Bad example: “This document will walk you through…”  
Amiga-appropriate rewrite: “Installation” followed by the steps.

**Anti-pattern: Excessive hedging boilerplate.**  
What it is: Overusing “may”, “might”, “could” everywhere as a blanket disclaimer.  
Why it reads AI: It becomes non-committal and vague; classic docs hedge where needed, but stay specific (what is tested, what is risky, what fails). citeturn10view3turn10view4turn3view3  
Bad example: “This might possibly help in some cases.”  
Amiga-appropriate rewrite: “On Kickstart 1.3, CPUs above 68020 are detected as 68020.” (Concrete limitation.) citeturn10view3

## Positive style guide for classic Amiga utility documentation

image_group{"layout":"carousel","aspect_ratio":"16:9","query":["Aminet package readme Short Author Uploader screenshot","Amiga Workbench README text file screenshot","AmigaGuide viewer documentation screenshot"],"num_per_query":1}

This section is a “do this” model, derived from recurring conventions in the corpus.

**Voice and audience assumptions**  
Write as an author/operator talking to technically capable users. Assume the reader understands drawers, assigns, running Installer, and basic CLI usage; explain only what is specific to *your* tool or what commonly trips people up. This matches the corpus where authors quickly transition from purpose to operation, and only add extra explanation when the underlying concept is non-trivial (e.g., the idea of “path assignments”). citeturn11view0turn11view2turn10view3

**Honesty and limits as first-class content**  
Classic readmes will state constraints without euphemism: incomplete features, untested setups, emulator uncertainty, or “I can’t reproduce this yet” are written plainly. This is visible in “Known problems” sections and “Limitations/Disclaimer” sections, and in warnings for hacky system-level tools. citeturn10view4turn10view3turn3view3turn11view1turn7view3turn7view0

**Canonical Aminet metadata header for Aminet-bound readmes**  
If the output is intended as an Aminet `.readme`, start with the machine-readable fields (and keep them in the expected order). The Aminet format explicitly describes these fields (e.g., “Short:”, “Author:”, “Uploader:”, “Type:”, “Version:”, “Architecture:”) and expects an empty line after them before the free-form documentation. The “Short:” field is constrained (first line, ≤40 characters, no boasting, no versions/platforms repeated). citeturn4view0

**Section order and “what readers look for”**  
Across utilities and system tools, sections commonly appear in this user-centred order:

Purpose / What it is (1–2 paragraphs), then Requirements, then Installation, then Usage (including examples), then Known problems/Limitations, then History/Changes, then Contents/Archive contents, then Contact/License (if needed). This ordering is explicit in multiple readmes and package pages. citeturn11view1turn11view2turn11view5turn11view4turn11view3turn9view2turn3view1turn2view5

**Sentence and paragraph tendencies**  
Use short-to-medium sentences; avoid long chains of clauses. Allow occasional informal asides, but only when they convey a real constraint or expectation (e.g., “not from Workbench ;-)”, or a wry note about a problem or workaround). Keep paragraphs short; use blank lines to separate concepts. citeturn11view2turn7view0turn2view4

**Formatting conventions (plain text first)**  
Use simple ASCII headings and separators: underlined headings (“Usage” followed by `~~~~~`), equals underlines (`=====`), or decorative brackets (`--+  Requirements  +--`). Use indentation for option descriptions and for lists of files/paths. Many readmes also include long underscore lines as separators. citeturn11view2turn11view1turn10view5turn11view0turn3view3turn10view1

**Instruction style**  
Write steps as imperatives with concrete destinations: copy to `C:`, `SYS:WBStartup/`, a named drawer, etc.; mention what backup is created and where; use the “RAM:” staging pattern when appropriate. citeturn11view5turn10view5turn11view4

**Commands, paths, and examples**  
Show real command lines and templates. When a command cannot run from Workbench, say so. When there is a template syntax, list it explicitly and then define each parameter. This is typical in CLI utilities. citeturn11view2turn11view1turn11view5

**Warnings and compatibility notes**  
Prefer direct markers (“WARNING:”, “NOTE:”, “Known problems:”, “Limitations/Disclaimer”). Include the precise trigger condition (“crashes if file not found”, “won’t work well with hardware X”, “only available for OS 3.2”, “requires bsdsocket.library”). citeturn7view0turn10view4turn11view1turn11view4turn10view3

**Bug disclosure style**  
A classic pattern is: state the problem, state the scope (reported vs reproduced), and state the workaround if any. Sometimes the author explicitly says they cannot reproduce yet. This is useful, credible, and matches archive norms. citeturn10view4turn7view3turn7view0

**Natural endings**  
Do not add summary conclusions. End after the last useful section. If needed, end with “Contact:” and an email, or with a short sign-off, or with license text. Many readmes do exactly that. citeturn11view4turn2view4turn3view3

## Structural templates and formatting patterns

These templates are designed to be filled by an AI without drifting into modern README boilerplate. They include deliberate “slots” for honest limits and tested scope, because that is both corpus-consistent and a practical guard against confident invention. citeturn10view3turn10view4turn9view8

**Short Aminet-style readme template** (plain text, Aminet fields first) citeturn4view0
```text
Short:        <<=40 chars; what it does; no boasting>
Author:       <name> [<email optional>]
Uploader:     <contact; non-obfuscated address per Aminet rules>
Type:         <aminet path, e.g. util/cli>
Version:      <x.y[.z]>
Architecture: <arch [>= version]; ...>
Requires:     <other archives; OS/Kickstart; RAM; chipset; libs>   (optional)
Replaces:     <full path(s)>                                       (optional)
Distribution: <Aminet|NoCD>                                       (optional)

<One paragraph: what it is / why it exists. Concrete nouns.>

Requirements
============
- <OS/Kickstart/CPU/RAM>
- <key libraries / external tools>

Installation
============
1. <copy/unpack/run Installer; exact destinations>
2. <prefs/env/tooltypes locations, if any>

Usage
=====
<CLI template or WB usage. Include 1–3 concrete examples.>

Known problems / Limitations
============================
- <symptom> (<scope: reproduced? hardware? OS?>) <workaround if any>
- <untested setups stated plainly>

History
=======
x.y.z (YYYY-MM-DD)
- <one-line change notes>
- <avoid marketing; include fixes and compatibility changes>

Contents
========
<key files/drawers; what each is>

Contact
=======
<email / where to report bugs>
```

**Installation section template** (copy/installer conventions) citeturn11view5turn10view5turn11view4
```text
Installation
============

Unpack the archive to <destination drawer>.

If an Installer script is included:
- Run the Installer icon.
- Note what gets backed up (and where).

If manual install:
- Copy <binary> to C:  (or wherever you keep CLI tools)
- Copy <WB tool> to SYS:Utilities/ (or SYS:WBStartup/ if it should auto-run)
- Copy libraries/classes to SYS:Libs/ or SYS:Classes/ as required

Prefs/Config:
- Stored in <ENVARC:...> and/or <PROGDIR:...>
```

**Usage section template** (CLI template + examples) citeturn11view2turn11view1turn11view4
```text
Usage
=====

CLI:
  <command> <args> [options]

Template:
  <ARG1/A>,<ARG2>,<FLAG/S>,<KEYWORD/K>,...

Arguments:
  ARG1/A   <what it is; required>
  ARG2     <what it is; optional>
  FLAG/S   <what it does>

Examples:
  <example 1>
  <example 2>

Workbench:
- <how to start it>
- Tooltypes:
  <TOOLTYPE>=<value>  (<default>)
```

**Compatibility notes template** (tested scope + explicit uncertainty) citeturn11view3turn7view3turn11view1turn10view4
```text
Compatibility
=============

Tested:
- <OS/Kickstart version> on <hardware/emulator> <notes>
- <libraries/stack> <notes>

Reported (not reproduced):
- <setup> -> <symptom>

Not tested:
- <setup list>

Notes:
- <performance/memory constraints if relevant>
```

**Known problems template** (problem → scope → workaround) citeturn10view4turn7view0turn7view3turn10view3
```text
Known problems
==============

- <problem statement>.
  Seen on: <OS/hardware>.
  Workaround: <what to do>, or "none yet".

- <limitation> (by design).
  Notes: <why>.
```

**History/version notes template** (compact, factual) citeturn11view4turn11view3turn2view4turn11view5
```text
History
=======

x.y.z (YYYY-MM-DD)
- Fixed: <bug>
- Changed: <behaviour/compat>
- Added: <feature>   (only if it is user-visible)

x.y (YYYY-MM-DD)
- First public release
```

**Archive contents template** (handwritten or tool-generated style) citeturn3view1turn2view5turn10view0
```text
Contents
========

<drawer>/
  <file>        - <what it is for>
  <file>        - <what it is for>

<binary>        - <purpose>
<doc>           - <documentation>
<installer>     - <Installer script>
```

**Short AmigaGuide intro template** (front page node + navigation) citeturn5view2turn12view0
```text
@database

@node MAIN "ToolName Help"
@toc MAIN
@next INSTALL

ToolName <version>

<One short paragraph: what it does.>

@{"Installation" LINK INSTALL}
@{"Usage" LINK USAGE}
@{"Known problems" LINK BUGS}
@{"History" LINK HISTORY}

@endnode

@node INSTALL "Installation"
<installation text>
@endnode
```

Optional tailoring rules (stretch goal):

Plain `README.txt` variant: drop Aminet-specific fields; replace with “Program:”, “Version:”, “Author:”, “Requires:”, “Install:”, “Usage:”, “History:”. Keep the same terse sectioning. citeturn11view5turn11view2turn11view4

AmigaGuide front-page variant: keep the first node as a “jump page” with links to the operational nodes; avoid long prose on MAIN; let nodes do the work. citeturn5view2turn12view0

Release-notes variant: lead with version/date, then tight bullet-like change lines; include “Fixed:” and “Known issues:” when relevant; avoid narrative. citeturn11view4turn11view3turn2view4

## Lexical guidance and phrasebook

This is a practical “lexicon” intended for both generation constraints and review. It is evidence-informed in two ways: (1) it reflects word/phrase patterns observed in the corpus (drawers, assigns, “requires”, “not tested”, “known problems”), and (2) it explicitly bans common AI tell phrases reported as stock patterns. citeturn11view2turn11view0turn10view3turn9view5turn9view4turn0search6

**Preferred wording patterns (good defaults)**  
“X is a small/simple <CLI/WB> tool that <verb>…” citeturn11view2turn9view1  
“I wrote it because I needed…” (only if you truly have a reason; it is common in the corpus and reads natural when specific). citeturn11view2turn3view0  
“Requires: Kickstart X / OS Y / <library>.” citeturn11view3turn11view4turn10view5turn10view3  
“Installation: Copy … to …” / “Run Installer …” citeturn11view5turn11view4turn10view5  
“Usage: …” + real examples. citeturn11view2turn11view1  
“Known problems:” / “Limitations/Disclaimer:” citeturn10view3turn10view4turn7view0turn7view3  
“Not tested on …” / “Reported: … (not reproduced)” citeturn7view3turn10view4turn11view1  
“Workaround: …” (or “none yet”). citeturn10view4turn7view0

**Discouraged wording patterns (replace with specifics)**  
“Designed to / aims to / provides a solution” → replace with “Does X” (one verb).  
“Seamless / robust / powerful / cutting-edge” → replace with constraints, requirements, or measured behaviour.  
“User-friendly / intuitive” → replace with UI/usage facts (hotkeys, tooltypes, menus). citeturn11view4turn10view5

**Banned or near-banned “AI smell” phrases** (use only if quoting an external doc)  
“delve into”, “at its core”, “it’s important to note”, “in today’s world”, “leverage”, “utilize”, “seamless integration”, “unlock”, “empower”. These are repeatedly flagged as telltale AI-stock phrasing and contribute to the polished-but-stiff register you are trying to avoid. citeturn9view5turn9view4

**Natural alternatives that fit archive readmes**  
Instead of “delve into”: “See”, “Read”, “Details below”. citeturn11view1turn11view0  
Instead of “it’s important to note”: “NOTE:” or “Warning:” plus the fact. citeturn7view0turn11view5  
Instead of “robust”: “Tested on …”, “works on …”, “fails on …”. citeturn11view3turn7view3turn10view4  
Instead of “streamline workflow”: “Saves typing by …”, “avoids …”, “replaces …”. citeturn11view0turn11view5turn10view2

## Operational prompts for generation and editing

These prompts are designed to operationalise the findings: they encode the positive model (structure + phrasing) and include explicit negative constraints that suppress AI tells. Negative constraints are especially useful because AI drafts often default to predictable, transition-heavy prose. citeturn9view4turn9view5turn0search6

**Reusable generation prompt (master prompt)**  
```text
You are writing plain-text documentation for a classic Amiga utility.
Audience: technically capable Amiga users. Tone: direct, practical, not sales-like.

Goal: produce an Aminet-style README and/or a bundled README.txt that would feel at home in a classic utility archive.
Avoid modern SaaS/blog tone. Avoid “in conclusion”, recaps, and stock transitions.

Hard constraints (do not violate):
- No marketing adjectives (“robust”, “seamless”, “cutting-edge”, “powerful”).
- No stock AI phrases: “delve into”, “at its core”, “it’s important to note”, “leverage”, “utilize”.
- No overly polite smoothing (“please make sure to…”). Use WARNING/NOTE if needed.
- No filler paragraphs that restate earlier content.
- Do not invent requirements, compatibility, commands, file names, or features.
  If information is missing, explicitly write “Unknown”, “Not tested”, or “Reported only” and keep it brief.
- Prefer concrete Amiga terms: drawer, assign, tooltypes, WBStartup, C:, S:, DEVS:, LIBS:, etc (only when relevant).

If targeting Aminet .readme format:
- Start with these fields (machine-readable header), then a blank line:
  Short:, Author:, Uploader:, Type:, Version:, Architecture:
  Add Requires:/Replaces:/Distribution: only if applicable.
- Keep Short: <= 40 characters and do not include version/platform.

Document structure (use these headings; omit sections that would be empty):
- Requirements
- Installation
- Usage
- Tooltypes (if WB tool)
- Known problems / Limitations
- History
- Contents (brief)
- Contact (optional)

Formatting:
- Plain ASCII. Keep lines roughly 72–78 columns where practical.
- Use simple headings with underlines (==== or ~~~~) or bracket style (--+  ...  +--).
- Use real command examples (CLI) and real destinations (C:, SYS:, ENVARC:, PROGDIR:).
- Keep paragraphs short. Prefer lists for options and requirements.

Inputs you will be given:
1) Program name and version
2) What it does (one-sentence summary)
3) Supported OS/Kickstart/CPU and known dependencies (if known)
4) Install method (Installer? manual copy? WBStartup?)
5) Usage syntax and options (if known)
6) Known problems / limitations / untested setups (if any)
7) Archive contents (key files)
8) Contact / licence notes (if any)

Output:
Return only the documentation text.
```

**Reusable editing prompt (rewrite prompt)**  
```text
Rewrite the following documentation into classic Amiga utility README style.

Priorities:
1) Preserve technical meaning. Do not drop constraints, file names, commands, or version facts.
2) Remove AI-sounding phrasing (stock transitions, generic “designed to” wording, marketing adjectives).
3) Make limitations and test scope explicit and plain.
4) Convert structure into practical sections: Requirements, Installation, Usage, Known problems/Limitations, History, Contents, Contact.
5) Prefer concrete Amiga terminology (drawers, assigns, tooltypes) when it clarifies an action.
6) Keep plain-text formatting, short paragraphs, and simple ASCII headings.

Rules:
- Do not add a concluding summary.
- Do not invent missing facts. If the original text is vague, keep it vague but honest (e.g., “not tested on X”).
- Replace “please” warnings with direct WARNING/NOTE lines.
- Replace abstract nouns (“functionality”, “solution”) with verbs and objects where possible.

Return:
- The rewritten documentation only.
- After the doc, add a short “Change notes” block that lists what you removed or condensed (no more than 8 lines).
```

## Validation checklist, before/after rewrites, and final house style specification

This section is meant to be used as a human review gate: if the AI draft fails these checks, it will read “off” to Amiga archive users even if technically correct. The checklist is also tuned to known AI failure modes (uniformity, repetition, confident invention). citeturn9view6turn0search6turn9view8turn9view4turn9view5

**Validation checklist (reviewer gate)**  
Does it start with the correct machine-readable Aminet fields when it claims to be an Aminet `.readme`, and is “Short:” actually short and non-boastful? citeturn4view0  
Does the first paragraph state what the tool does in concrete terms (not “designed to…”)? citeturn11view2turn11view5  
Are Requirements specific (OS/Kickstart/CPU/RAM/libs/tools), not vague? citeturn11view3turn11view4turn10view3turn10view5  
Are Installation steps written as direct actions with real destinations (C:, SYS:WBStartup/, ENVARC:…)? citeturn11view5turn11view4turn10view5  
Does Usage include a real syntax/template and at least one example, if it’s a CLI tool? citeturn11view2turn11view1  
Are Known problems / Limitations present when there are real risks, and are they stated plainly (including “not tested” where appropriate)? citeturn10view4turn7view3turn10view3turn11view1  
Is the tone practical and occasionally candid, without being “friendly” for its own sake? citeturn10view4turn11view2turn2view4turn7view0  
Did the AI avoid stock phrases and transition glue? citeturn9view4turn9view5  
Is there any paragraph that could be deleted without losing actionable information? If yes, delete it.  
Are there any claims that look like guesswork presented as fact? If yes, rewrite as tested scope or “unknown/not tested”—models can hallucinate confidently. citeturn9view8  
Does the document end naturally (after History/Contact/etc.) without a recap conclusion? citeturn11view4turn2view5turn2view4

**Before/after examples (AI-sounding → Amiga-appropriate)**

One-sentence fix:  
Bad (AI-ish): “This powerful utility streamlines your workflow by leveraging advanced features for seamless operation.”  
Better (Amiga utility style): “Small utility for <task>. Requires <OS/lib>. See ‘Usage’ below.”  
What changed: removed marketing adjectives and vague nouns; added concrete task + requirement pointer (typical of archive readmes). citeturn11view2turn11view5turn11view3

Paragraph fix:  
Bad (AI-ish):  
“Welcome to ToolName! This guide will walk you through installation and usage. First, download the package, then extract it, and finally follow the steps below. In conclusion, you’ll be ready to use ToolName efficiently.”  
Better (Amiga utility style):  
“ToolName does <task>. It was written to <specific reason, optional>.  
Installation: unpack the archive, run Installer (or copy ToolName to C:).  
Usage: see examples below. No recap.”  
What changed: removed “welcome”/meta narration and the conclusion; replaced with immediate purpose + direct install action; matched the corpus pattern of getting to work quickly. citeturn11view2turn11view5turn10view5

Full section rewrite: Known problems  
Bad (AI-ish):  
“Known Issues: At this time, there are no known issues. If you encounter any problems, please feel free to reach out. We are committed to improving the experience.”  
Better (Amiga utility style):  
“Known problems:  
- Not tested on <setup>.  
- Reported: <symptom> on <hardware>. Not reproduced yet.  
- If <file> is missing, it may crash. Put it in the same drawer as the program.”  
What changed: replaced empty reassurance with concrete scope and triggers; added the “reported vs reproduced” distinction common in classic docs; removed corporate support voice. citeturn7view3turn10view4turn7view0

**Final house style specification (condensed “one document to rule them all”)**

Purpose: Write plain-text documentation for Amiga utilities that looks native to classic archives: terse, technically literate, and honest about limits. citeturn11view2turn10view3turn4view0

Format: Prefer plain ASCII with simple headings (underlines, bracket banners). Keep paragraphs short; wrap lines sensibly; use indentation for options and lists. citeturn11view2turn11view1turn10view5turn7view0

Aminet compliance: If producing an Aminet `.readme`, start with header fields; keep “Short:” ≤40 chars and non-boastful; follow with an empty line and then the doc. citeturn4view0

Required content (when applicable):  
What it is (1 paragraph), Requirements (explicit OS/Kickstart/CPU/RAM/libs), Installation (copy/Installer steps with destinations), Usage (syntax + examples; Tooltypes for WB tools), Known problems/Limitations (plain, specific, with test scope), History (version/date bullets), Contents (key files), Contact (optional). citeturn11view4turn11view5turn11view2turn11view3turn10view3turn10view4turn3view1turn2view5

Tone rules:  
No marketing. No “welcome”. No pep. No conclusions. Use WARNING/NOTE when needed. Say “not tested” when you don’t know. Avoid generic abstractions. citeturn10view3turn10view4turn9view8turn9view5turn9view4

Language bans:  
Avoid stock AI phrases and transition glue; avoid “designed to” boilerplate. Prefer direct verbs and concrete nouns. citeturn9view5turn9view4turn0search6