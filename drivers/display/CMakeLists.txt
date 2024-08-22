# Copyright (c) 2024 DxPru. All Rights Reserved.

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_DISPLAY_HUB75_PIO hub75_pio.c)
