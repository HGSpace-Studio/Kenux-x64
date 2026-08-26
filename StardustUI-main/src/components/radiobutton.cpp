#include "../../includes/components/radiobutton.hpp"
#include "../../includes/theme.hpp"
#include "../../includes/vector.hpp"
#include "../../platforms/platform.hpp"

namespace {
int max_int(int a, int b) {
    return a > b ? a : b;
}

int min_int(int a, int b) {
    return a < b ? a : b;
}

stardustui::vector<RadioButton*> g_radio_registry;

bool text_equals(const stardustui::string& left, const stardustui::string& right) {
    return left.equals(right.c_str());
}

Sytel resolve_radio_style(const base_component& component) {
    Sytel style = stardustui::Theme::component_style("radio");
    style.merge_from(component.resolve_style());
    return style;
}
}

RadioButton::RadioButton(const stardustui::string& text, int width, int height)
    : text(text),
      group(),
      selected(false),
      change_callback_func(nullptr) {
    this->set_bounds(0, 0, width, height);
    g_radio_registry.push_back(this);
}

RadioButton::RadioButton(const stardustui::string& text, int width, int height, const SytelRules& style)
    : text(text),
      group(),
      selected(false),
      change_callback_func(nullptr) {
    this->set_bounds(0, 0, width, height);
    this->set_style_rules(style);
    g_radio_registry.push_back(this);
}

RadioButton::~RadioButton() {
    for (int index = 0; index < g_radio_registry.size(); ++index) {
        if (g_radio_registry[index] == this) {
            g_radio_registry[index] = nullptr;
            break;
        }
    }
}

void RadioButton::draw(unsigned long long handle) {
    const Sytel style = resolve_radio_style(*this);
    const unsigned int border_width = style.get_border_width(2);
    const unsigned int base_border_color = style.get_border_color(0x7A757FFF);
    const unsigned int base_background_color = style.get_background_color(0xFFFBFFFF);
    const unsigned int accent_color = style.get_color(0x6750A4FF);
    const unsigned int text_size = style.get_size(16);
    const unsigned int padding = style.get_padding(10);
    unsigned int border_color = base_border_color;
    unsigned int background_color = base_background_color;
    unsigned int label_color = this->selected ? accent_color : stardustui::Theme::colors().on_surface;

    if (this->is_hover_active()) {
        border_color = accent_color;
        if (!this->selected) {
            label_color = accent_color;
        }
    }
    if (this->is_click_active()) {
        border_color = accent_color;
        background_color = stardustui::Theme::colors().surface_variant;
        label_color = accent_color;
    }

    const int indicator_size = this->get_indicator_size(text_size, padding);
    const int indicator_y = static_cast<int>(this->y) + max_int(0, (this->get_height() - indicator_size) / 2);
    const unsigned int radius = static_cast<unsigned int>(indicator_size / 2);

    draw_round_rect(handle,
                    static_cast<int>(this->x),
                    indicator_y,
                    indicator_size,
                    indicator_size,
                    radius,
                    border_color);

    const int inner_x = static_cast<int>(this->x) + static_cast<int>(border_width);
    const int inner_y = indicator_y + static_cast<int>(border_width);
    const int inner_size = indicator_size - static_cast<int>(border_width * 2);
    if (inner_size > 0) {
        const unsigned int inner_radius = static_cast<unsigned int>(inner_size / 2);
        draw_round_rect(handle, inner_x, inner_y, inner_size, inner_size, inner_radius, background_color);
    }

    if (this->selected && inner_size > 6) {
        const int dot_size = max_int(6, inner_size / 2);
        const int dot_x = static_cast<int>(this->x) + (indicator_size - dot_size) / 2;
        const int dot_y = indicator_y + (indicator_size - dot_size) / 2;
        const unsigned int dot_color = this->is_click_active()
            ? stardustui::Theme::colors().primary_container
            : accent_color;
        draw_round_rect(handle,
                        dot_x,
                        dot_y,
                        dot_size,
                        dot_size,
                        static_cast<unsigned int>(dot_size / 2),
                        dot_color);
    }

    const int text_x = static_cast<int>(this->x) + indicator_size + static_cast<int>(padding);
    const int text_y = static_cast<int>(this->y) + max_int(0, (this->get_height() - static_cast<int>(calc_text_height(this->text, text_size))) / 2);
    draw_text(handle, text_x, text_y, label_color, text_size, this->text);
}

int RadioButton::get_preferred_width() const {
    const Sytel style = resolve_radio_style(*this);
    const unsigned int text_size = style.get_size(16);
    const unsigned int padding = style.get_padding(10);
    return this->get_indicator_size(text_size, padding) + static_cast<int>(padding) +
           static_cast<int>(calc_text_width(this->text, text_size));
}

int RadioButton::get_preferred_height() const {
    return this->get_height();
}

bool RadioButton::handle_left_button(bool pressed, int x, int y) {
    const bool inside = this->contains(x, y);
    if (pressed) {
        const bool changed = this->set_focus(inside);
        this->set_click_state(inside);
        if (inside) {
            this->set_mouse_state(true);
            this->set_hover_state(true);
        }
        return changed || inside;
    }

    const bool was_active = this->is_click_active();
    this->set_click_state(false);
    if (was_active && inside) {
        const bool changed = this->set_selected_internal(true, true);
        this->run_callback();
        return changed || was_active;
    }
    return was_active;
}

void RadioButton::set_text(const stardustui::string& text) {
    this->text = text;
    this->request_redraw();
}

const stardustui::string& RadioButton::get_text() const {
    return this->text;
}

void RadioButton::set_group(const stardustui::string& group) {
    this->group = group;
    if (this->selected) {
        this->clear_group_selection(false);
    }
}

const stardustui::string& RadioButton::get_group() const {
    return this->group;
}

void RadioButton::set_selected(bool selected) {
    this->set_selected_internal(selected, false);
}

bool RadioButton::is_selected() const {
    return this->selected;
}

void RadioButton::set_change_callback(void (*func)(RadioButton&, bool)) {
    this->change_callback_func = func;
}

bool RadioButton::set_selected_internal(bool selected, bool notify) {
    if (this->selected == selected) {
        return false;
    }

    if (selected) {
        this->clear_group_selection(false);
    }

    this->selected = selected;
    this->request_redraw();
    if (notify && this->change_callback_func != nullptr) {
        this->change_callback_func(*this, this->selected);
    }
    return true;
}

void RadioButton::clear_group_selection(bool notify) {
    if (this->group.length() <= 0) {
        return;
    }

    for (int index = 0; index < g_radio_registry.size(); ++index) {
        RadioButton* peer = g_radio_registry[index];
        if (peer == nullptr || peer == this) {
            continue;
        }
        if (!text_equals(peer->group, this->group)) {
            continue;
        }
        peer->set_selected_internal(false, notify);
    }
}

int RadioButton::get_indicator_size(unsigned int text_size, unsigned int padding) const {
    const int desired = static_cast<int>(text_size) + max_int(4, static_cast<int>(padding / 2));
    if (this->get_height() <= 0) {
        return desired;
    }
    return min_int(this->get_height(), desired);
}
