# Bootloader Updater

On supported boards, selecting the `Zephyr loader` programmer and running
`Burn Bootloader` uploads this updater to restore or update the board's original
bootloader without an external programmer.

The updater is written to the internal flash in place of the Zephyr loader and it contains an image of the board's original bootloader. When the updater starts after the upload, it compares this image with the installed bootloader and, if they differ, erases and rewrites the bootloader partition without saving a backup of its previous contents.

> [!IMPORTANT]
> **Do not reset the board or disconnect its power while the update is in progress.**
> Interrupting the update may leave the board unable to start and require an
> external programmer to recover it.

The LED blinks slowly when the update has completed successfully, or quickly if
it has failed.

After a successful update, double-tap RESET to enter bootloader mode and
**upload the sketch that was running before the update again**. This also
reinstalls the Zephyr loader so the sketch can run.
