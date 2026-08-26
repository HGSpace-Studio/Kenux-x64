#include "../../includes/components/checkbox.hpp"
#include "../../includes/theme.hpp"
#include "../../platforms/platform.hpp"

namespace {
int max_int(int a, int b) {
    return a > b ? a : b;
}

int min_int(int a, int b) {
    return a < b ? a : b;
}

Sytel resolve_checkbox_style(const base_component& component) {
    Sytel style = stardustui::Theme::component_style("checkbox");
    style.merge_from(component.resolve_style());
    return style;
}

void draw_line(unsigned long long handle, int x0, int y0, int x1, int y1, unsigned int color, int thickness) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int steps = max_int(dx < 0 ? -dx : dx, dy < 0 ? -dy : dy);
    if (steps <= 0) {
        draw_rect(handle, x0, y0, max_int(1, thickness), max_int(1, thickness), color);
        return;
    }

    for (int step = 0; step <= steps; ++step) {
        const int x = x0 + (dx * step) / steps;
        const int y = y0 + (dy * step) / steps;
        draw_rect(handle, x, y, max_int(1, thickness), max_int(1, thickness), color);
    }
}
}

Checkbox::Checkbox(const stardustui::string& text, int width, int height)
    : text(text),
      checked(false),
      toggle_callback_func(nullptr) {
    this->set_bounds(0, 0, width, height);
}

Checkbox::Checkbox(const stardustui::string& text, int width, int height, const SytelRules& style)
    : text(text),
      checked(false),
      toggle_callback_func(nullptr) {
    this->set_bounds(0, 0, width, height);
    this->set_style_rules(style);
}

Checkbox::~Checkbox() = default;

void Checkbox::draw(unsigned long long handle) {
    const Sytel style = resolve_checkbox_style(*this);
    const unsigned int border_width = style.get_border_width(2);
    const unsigned int base_border_color = style.get_border_color(0x7A757FFF);
    const unsigned int base_background_color = style.get_background_color(0xFFFBFFFF);
    const unsigned int accent_color = style.get_color(0x6750A4FF);
    const unsigned int text_size = style.get_size(16);
    const unsigned int padding = style.get_padding(10);
    const unsigned int radius = style.get_radius(6);
    unsigned int border_color = base_border_color;
    unsigned int background_color = base_background_color;
    unsigned int label_color = this->checked ? accent_color : stardustui::Theme::colors().on_surface;

    if (this->is_hover_active()) {
        border_color = accent_color;
        if (!this->checked) {
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
        const unsigned int inner_radius = radius > border_width ? radius - border_width : 0;
        unsigned int inner_color = this->checked ? accent_color : background_color;
        if (this->checked && this->is_click_active()) {
            inner_color = stardustui::Theme::colors().primary_container;
        }
        draw_round_rect(handle, inner_x, inner_y, inner_size, inner_size, inner_radius, inner_color);
    }

    if (this->checked && inner_size > 4) {
        const unsigned int check_color = this->is_click_active()
            ? stardustui::Theme::colors().primary
            : stardustui::Theme::colors().on_primary;
        const int stroke = max_int(1, indicator_size / 7);
        const int left_x = static_cast<int>(this->x) + indicator_size / 4;
        const int left_y = indicator_y + indicator_size / 2;
        const int mid_x = static_cast<int>(this->x) + indicator_size / 2 - 1;
        const int mid_y = indicator_y + (indicator_size * 3) / 4;
        const int right_x = static_cast<int>(this->x) + (indicator_size * 3) / 4;
        const int right_y = indicator_y + indicator_size / 4;
        draw_line(handle, left_x, left_y, mid_x, mid_y, check_color, stroke);
        draw_line(handle, mid_x, mid_y, right_x, right_y, check_color, stroke);
    }

    const int text_x = static_cast<int>(this->x) + indicator_size + static_cast<int>(padding);
    const int text_y = static_cast<int>(this->y) + max_int(0, (this->get_height() - static_cast<int>(calc_text_height(this->text, text_size))) / 2);
    draw_text(handle, text_x, text_y, label_color, text_size, this->text);
}

int Checkbox::get_preferred_width() const {
    const Sytel style = resolve_checkbox_style(*this);
    const unsigned int text_size = style.get_size(16);
    const unsigned int padding = style.get_padding(10);
    return this->get_indicator_size(text_size, padding) + static_cast<int>(padding) +
           static_cast<int>(calc_text_width(this->text, text_size));
}

int Checkbox::get_preferred_height() const {
    return this->get_height();
}

bool Checkbox::handle_left_button(bool pressed, int x, int y) {
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
        this->set_checked_internal(!this->checked, true);
        this->run_callback();
        this->request_redraw();
        return true;
    }
    return was_active;
}

void Checkbox::set_text(const stardustui::string& text) {
    this->text = text;
    this->request_redraw();
}

const stardustui::string& Checkbox::get_text() const {
    return this->text;
}

void Checkbox::set_checked(bool checked) {
    this->set_checked_internal(checked, false);
}

bool Checkbox::is_checked() const {
    return this->checked;
}

void Checkbox::set_toggle_callback(void (*func)(Checkbox&, bool)) {
    this->toggle_callback_func = func;
}

bool Checkbox::set_checked_internal(bool checked, bool notify) {
    if (this->checked == checked) {
        return false;
    }

    this->checked = checked;
    this->request_redraw();
    if (notify && this->toggle_callback_func != nullptr) {
        this->toggle_callback_func(*this, this->checked);
    }
    return true;
}

int Checkbox::get_indicator_size(unsigned int text_size, unsigned int padding) const {
    const int desired = static_cast<int>(text_size) + max_int(4, static_cast<int>(padding / 2));
    if (this->get_height() <= 0) {
        return desired;
    }
    return min_int(this->get_height(), desired);
}
