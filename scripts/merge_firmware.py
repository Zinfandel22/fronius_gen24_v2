# Tell the IDE linter to ignore PlatformIO's SCons injected variables
Import("env")  # type: ignore

APP_BIN = "$BUILD_DIR/${PROGNAME}.bin"
MERGED_BIN = "$PROJECT_DIR/${PROGNAME}_merged.bin"

BOARD_CONFIG = env.BoardConfig()  # type: ignore

def merge_bin(source, target, env):
    print("\n--- Starting ESP32-S3 Firmware Merge ---")
    
    # 1. Automatically grab the bootloader (0x0), partitions (0x8000), and boot_app0 (0xe000)
    flash_images = env.Flatten(env.get("FLASH_EXTRA_IMAGES", []))
    
    # 2. Add the main application firmware
    app_offset = env.get("ESP32_APP_OFFSET", "0x10000")
    flash_images += [app_offset, APP_BIN]

    # 3. Construct and execute the esptool merge_bin command
    cmd = [
        "$PYTHONEXE", "$OBJCOPY",
        "--chip", BOARD_CONFIG.get("build.mcu", "esp32s3"),
        "merge_bin",
        "-o", MERGED_BIN,
        "--flash_mode", "keep",
        "--flash_size", BOARD_CONFIG.get("build.flash_size", "16MB"),
    ] + flash_images
    
    # Execute the merge command
    env.Execute(" ".join(cmd))
    
    merged_path = env.subst(MERGED_BIN)
    print("--- Merge Complete! ---")
    print(f"Your optimized .bin is located at: {merged_path}\n")

env.AddPostAction(APP_BIN, merge_bin)  # type: ignore