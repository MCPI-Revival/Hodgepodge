#pragma once

extern int hotbar_offset;
extern bool hotbar_needs_update;

void set_hotbar(Inventory *inv);
Screen_vtable *get_inventory_screen_vtable();
