# Michat emoji directory

Put the emoji images that users may choose here. The chat UI opens this directory when the **Emoji** button is pressed.

For reliable client display, use PNG images no larger than 280×210 pixels. The current root-level images have been normalized to this format and size. Their unchanged originals are retained in `originals/` as a recoverable backup; the picker does not scan subdirectories.

The picker also accepts BMP and standard (baseline) JPG/JPEG up to 5 MB. A progressive JPEG remains selectable and sendable, but is labelled “Preview unavailable” because the bundled LVGL decoder cannot render that JPEG encoding. The image filename is sent with the existing independent `emoji` command; it is not inserted into the text-message input and it is not selected through the ordinary file-path controls.

For a temporary alternate directory during development, start the client with `MICHAT_EMOJI_DIR` set to an absolute directory path. This does not change the project default.
