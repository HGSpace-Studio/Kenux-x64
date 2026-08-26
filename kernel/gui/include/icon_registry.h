#ifndef ICON_REGISTRY_H
#define ICON_REGISTRY_H

#include "icon.h"

typedef struct {
    const char* name;
    icon_id_t id;
    const char* category;
} icon_registry_entry_t;

const icon_registry_entry_t* icon_registry_get(size_t* count);
icon_id_t icon_registry_lookup(const char* name);
const char* icon_registry_get_name(icon_id_t id);
const char* icon_registry_get_category(icon_id_t id);
size_t icon_registry_count_by_category(const char* category);

#endif
