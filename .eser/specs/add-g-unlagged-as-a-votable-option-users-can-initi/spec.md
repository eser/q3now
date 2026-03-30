# Spec: add-g-unlagged-as-a-votable-option-users-can-initi

## Status: done

## Concerns: beautiful-product, long-lived, move-fast

## Discovery Answers

### status_quo

{"status_quo": "Today only server admins can change g_unlagged via rcon. Players cannot toggle it during a match. This is frustrating when lag compensation is causing issues and players want to try without it, or vice versa.", "ambition": "1-star: raw cvar vote with no UI. 10-star: full UI integration in both q3_ui and Wired callvote menus with human-readable display strings. 5-star MVP: console callvote support + at least one UI. We aim for 8-star: console + both UIs.", "trigger": "A player types callvote g_unlagged 0 or callvote g_unlagged 1 in console, or clicks a button in the callvote menu.", "risks": "Low risk. Follows exact same pattern as shuffle and weather votes. g_unlagged is already CVAR_SERVERINFO so clients see the change immediately.", "verification": "Test: callvote g_unlagged 0, callvote g_unlagged 1, invalid values rejected, UI buttons work in both q3_ui and Wired menu, vote string displays correctly.", "scope": "Server-side: add g_unlagged to vote whitelist in g_cmds.c with value validation (0 or 1 only). Client-side: add button to q3_ui/ui_callvote.c and modfiles/ui/callvote.menu. Display string shows Enable/Disable Unlagged based on voted value."}

### ambition

{"ambition": "10-star: console callvote g_unlagged 0/1 with value validation + buttons in both q3_ui callvote menu and Wired UI callvote menu. Display string shows Enable/Disable Unlagged. MVP (5-star) would be console-only, but we aim for full coverage."}

### reversibility

{"reversibility": "Fully reversible. Adding g_unlagged to the vote whitelist is a code addition only. Players can vote to toggle it back anytime. No data migration, no schema change. The existing 30-second callvote cooldown prevents spam."}

### user_impact

{"user_impact": "Purely additive. No existing behavior changes. Players gain a new callvote option. Server g_unlagged default (1) unchanged. No breaking changes."}

### verification

{"verification": "Manual in-game testing: 1) callvote g_unlagged 0 works and displays Enable/Disable Unlagged correctly, 2) callvote g_unlagged 1 works, 3) callvote g_unlagged 2 is rejected with error message, 4) callvote g_unlagged (no arg) is rejected, 5) q3_ui callvote menu button works, 6) Wired UI callvote menu button works, 7) vote passes and g_unlagged actually changes."}

### scope_boundary

{"scope_boundary": "Out of scope: generic cvar voting system, g_voteFlags mechanism, admin-only override protection, map_restart on toggle, any changes to the unlagged implementation itself. This feature ONLY adds g_unlagged to the callvote whitelist with value validation and UI buttons. No tech debt introduced — follows exact established pattern."}

## v1 vs v2 Scope (move-fast)

_To be addressed during execution._

## Out of Scope

- {"scope_boundary": "Out of scope: generic cvar voting system, g_voteFlags mechanism, admin-only override protection, map_restart on toggle, any changes to the unlagged implementation itself
- This feature ONLY adds g_unlagged to the callvote whitelist with value validation and UI buttons
- No tech debt introduced — follows exact established pattern."}

## Tasks

- [x] task-1: Add g_unlagged to the callvote whitelist in g_cmds.c Cmd_CallVote_f — add else-if branch accepting g_unlagged with arg validation (0 or 1 only), reject invalid values with error message, format voteString as g_unlagged 0/1 and voteDisplayString as Enable Unlagged or Disable Unlagged based on voted value. Update the help text string listing valid vote commands.
- [x] task-2: Add Unlagged toggle button to q3_ui callvote menu in code/q3_ui/ui_callvote.c — add ID_UNLAGGED define, add menutext_s unlagged field to callvoteMenu_t struct, wire up event handler to exec callvote g_unlagged 0 or 1 (toggling current value), add menu item in UI_CallVoteMenu_Init.
- [x] task-3: Add Unlagged toggle button to Wired UI callvote menu in modfiles/ui/callvote.menu — add a BUTTON entry for Enable/Disable Unlagged following the existing button pattern, exec callvote g_unlagged 0 or 1.

## Verification

- {"verification": "Manual in-game testing: 1) callvote g_unlagged 0 works and displays Enable/Disable Unlagged correctly, 2) callvote g_unlagged 1 works, 3) callvote g_unlagged 2 is rejected with error message, 4) callvote g_unlagged (no arg) is rejected, 5) q3_ui callvote menu button works, 6) Wired UI callvote menu button works, 7) vote passes and g_unlagged actually changes."}
