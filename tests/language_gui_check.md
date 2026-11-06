# Language switching: manual GUI checks

Run these checks only after building the candidate in a disposable Windows test
account or VM. They are not automated and must not be run against a working
session. Record the Windows display language, the application language, and the
candidate version. Preserve the user's original settings and autostart entry.

1. Launch the application without a saved language preference. Check the
   intended system-language default. Select English, Russian and Spanish in
   turn, then close and relaunch the application to verify each saved choice.
   Also verify a system language without a translation falls back to English.
2. Open Advanced settings and select a readable range setting. Enter a visibly
   different AC draft and a different DC draft without pressing Apply. Note the
   selected scheme, setting, cursor/selection, scroll position and draft values.
   Request a different interface language from the tray menu. Decline the
   discard confirmation and verify that both drafts and the language remain
   unchanged. Request the change again and explicitly confirm discarding the
   drafts; verify that labels, title, description and tray tooltip update.
3. Repeat with a named-choice setting: prepare different pending choices for
   AC and DC, decline discarding them, and verify the choices remain. Repeat
   with a setting whose current value has no named choice and with
   a read failure if these situations can be safely simulated in the test VM.
4. Cancel or discard the drafts. Verify that changing language never changes
   the active scheme, AC/DC values, or autostart status. Check each timeout
   submenu, disabled/unavailable text, confirmations, labels, and undo caption
   in all languages. Use existing test-only temporary schemes when a change or
   undo is explicitly required; do not modify a real scheme for this check.
5. With the tray menu open, a discard confirmation open, and the custom-time
   dialog open in turn, attempt to request another action or relaunch settings.
   Confirm no nested menu or language change loses data, no stale menu command
   is applied, and the editor remains usable after the dialog/menu closes.
6. Exercise enabled, disabled, moved and foreign autostart entries only in an
   isolated test account/VM using temporary entries; verify translated warnings
   preserve the exact original conflict and confirmation semantics. Verify a
   preference storage failure is reported without changing the autostart entry.

Windows-provided scheme names, descriptions, units and system error text may
remain in the Windows language. Document that separately from untranslated text
owned by PWSwitch. If a check fails, record the steps and observed state before
cleanup; do not attempt to repair the user's power settings from this plan.
