#include "../../includes/window.hpp"
#include "../../includes/theme.hpp"
#include "../../includes/sytle.hpp"
#include "../../includes/components/button.hpp"
#include "../../includes/components/checkbox.hpp"
#include "../../includes/components/flex.hpp"
#include "../../includes/components/lable.hpp"
#include "../../includes/components/radiobutton.hpp"
#include "../../includes/components/scrollbar.hpp"
#include "../../includes/components/textbox.hpp"

namespace {
FlexLayout* g_root_layout = nullptr;
FlexLayout* g_controls_layout = nullptr;
FlexLayout* g_code_panel_layout = nullptr;
Lable* g_title_label = nullptr;
Lable* g_subtitle_label = nullptr;
Lable* g_section_controls_label = nullptr;
Lable* g_section_code_label = nullptr;
Lable* g_status_label = nullptr;
Button* g_demo_button = nullptr;
TextBox* g_code_box = nullptr;
TextBox* g_preview_text = nullptr;
Checkbox* g_feature_checkbox = nullptr;
Checkbox* g_round_checkbox = nullptr;
RadioButton* g_theme_light = nullptr;
RadioButton* g_theme_dark = nullptr;
RadioButton* g_theme_green = nullptr;

void set_status(const char* text) {
    if (g_status_label != nullptr) {
        g_status_label->set_text(text);
    }
}

const char* selected_theme_name() {
    if (g_theme_dark != nullptr && g_theme_dark->is_selected()) {
        return "md3-dark";
    }
    if (g_theme_green != nullptr && g_theme_green->is_selected()) {
        return "green_light";
    }
    return "md3-light";
}

SytelRules make_snapshot_rules(const char* name) {
    SytelRules rules;
    rules.set_base_sytel(stardustui::Theme::component_style(name));
    return rules;
}

SytelRules make_label_rules(unsigned int size, unsigned int color) {
    Sytel style;
    style.set_size(size);
    style.set_color(color);

    SytelRules rules;
    rules.set_base_sytel(style);
    return rules;
}

void apply_showcase_theme() {
    const stardustui::Colors& colors = stardustui::Theme::colors();

    if (g_root_layout != nullptr) {
        g_root_layout->set_style_rules(make_snapshot_rules("panel"));
        g_root_layout->request_redraw();
    }
    if (g_controls_layout != nullptr) {
        g_controls_layout->set_style_rules(make_snapshot_rules("panel"));
        g_controls_layout->request_redraw();
    }
    if (g_code_panel_layout != nullptr) {
        g_code_panel_layout->set_style_rules(make_snapshot_rules("panel"));
        g_code_panel_layout->request_redraw();
    }
    if (g_demo_button != nullptr) {
        g_demo_button->set_style_rules(make_snapshot_rules("button"));
        g_demo_button->request_redraw();
    }
    if (g_feature_checkbox != nullptr) {
        g_feature_checkbox->set_style_rules(make_snapshot_rules("checkbox"));
        g_feature_checkbox->request_redraw();
    }
    if (g_round_checkbox != nullptr) {
        g_round_checkbox->set_style_rules(make_snapshot_rules("checkbox"));
        g_round_checkbox->request_redraw();
    }
    if (g_theme_light != nullptr) {
        g_theme_light->set_style_rules(make_snapshot_rules("radio"));
        g_theme_light->request_redraw();
    }
    if (g_theme_dark != nullptr) {
        g_theme_dark->set_style_rules(make_snapshot_rules("radio"));
        g_theme_dark->request_redraw();
    }
    if (g_theme_green != nullptr) {
        g_theme_green->set_style_rules(make_snapshot_rules("radio"));
        g_theme_green->request_redraw();
    }
    if (g_preview_text != nullptr) {
        g_preview_text->set_style_rules(make_snapshot_rules("textbox"));
        g_preview_text->request_redraw();
    }
    if (g_code_box != nullptr) {
        g_code_box->set_style_rules(make_snapshot_rules("code_block"));
        g_code_box->request_redraw();
    }
    if (g_title_label != nullptr) {
        g_title_label->set_style_rules(make_label_rules(28, colors.on_surface));
        g_title_label->request_redraw();
    }
    if (g_subtitle_label != nullptr) {
        g_subtitle_label->set_style_rules(make_label_rules(16, colors.on_surface_variant));
        g_subtitle_label->request_redraw();
    }
    if (g_section_controls_label != nullptr) {
        g_section_controls_label->set_style_rules(make_label_rules(18, colors.on_surface));
        g_section_controls_label->request_redraw();
    }
    if (g_section_code_label != nullptr) {
        g_section_code_label->set_style_rules(make_label_rules(18, colors.on_surface));
        g_section_code_label->request_redraw();
    }
    if (g_status_label != nullptr) {
        g_status_label->set_style_rules(make_label_rules(16, colors.on_surface_variant));
        g_status_label->request_redraw();
    }
}

void update_showcase_code() {
    if (g_code_box == nullptr) {
        return;
    }

    stardustui::string code;
    code.assign(
        "stardustui::Theme::load_theme(\"");
    code.append(selected_theme_name());
    code.append("\");\n\n");
    code.append("Checkbox feature_toggle(\"Enable advanced card\", 0, 36);\n");
    code.append("feature_toggle.set_checked(");
    code.append(g_feature_checkbox != nullptr && g_feature_checkbox->is_checked() ? "true" : "false");
    code.append(");\n\n");
    code.append("Checkbox rounded_toggle(\"Rounded corners from theme\", 0, 36);\n");
    code.append("rounded_toggle.set_checked(");
    code.append(g_round_checkbox != nullptr && g_round_checkbox->is_checked() ? "true" : "false");
    code.append(");\n\n");
    code.append("RadioButton theme_light(\"Light\", 0, 36);\n");
    code.append("RadioButton theme_dark(\"Dark\", 0, 36);\n");
    code.append("RadioButton theme_green(\"Green\", 0, 36);\n");
    code.append("theme_light.set_group(\"theme\");\n");
    code.append("theme_dark.set_group(\"theme\");\n");
    code.append("theme_green.set_group(\"theme\");\n\n");
    code.append("TextBox code_box(0, 0, false);\n");
    code.append("code_box.set_text(\"...generated example code...\");\n");
    g_code_box->set_text(code);
}

void on_button_click() {
    set_status("Showcase button clicked.");
}

void on_checkbox_changed(Checkbox&, bool) {
    update_showcase_code();
    set_status("Checkbox state updated.");
}

void on_radio_changed(RadioButton&, bool selected) {
    if (!selected) {
        return;
    }

    stardustui::Theme::load_theme(selected_theme_name());
    apply_showcase_theme();
    update_showcase_code();
    set_status("Theme preset changed.");
}
}

static int showcase_main_impl(int argc, char** argv, char**) {
    const char* theme_to_load = "md3-light";
    if (argc > 1 && argv[1] != nullptr && argv[1][0] != '\0') {
        theme_to_load = argv[1];
    }
    stardustui::Theme::load_theme(theme_to_load);
    const stardustui::Colors& colors = stardustui::Theme::colors();

    Window window("StardustUI Widget Showcase", 1180, 760, true);

    FlexLayout root(1140, 720);
    root.set_pos(20, 20);
    root.set_direction(FlexLayout::Row);
    root.set_gap(20);
    root.set_padding(20);
    root.set_anchors(base_component::AnchorLeft | base_component::AnchorTop |
                     base_component::AnchorRight | base_component::AnchorBottom);
    g_root_layout = &root;

    FlexLayout controls(360, 0);
    controls.set_direction(FlexLayout::Column);
    controls.set_gap(14);
    controls.set_padding(18);
    g_controls_layout = &controls;

    Lable title("Widget Gallery", 28, colors.on_surface);
    Lable subtitle("Checkbox / RadioButton / Theme tokens", 16, colors.on_surface_variant);
    Lable section_controls("Controls", 18, colors.on_surface);
    Lable section_code("Generated Example Code", 18, colors.on_surface);
    Lable status("Ready.", 16, colors.on_surface_variant);
    g_title_label = &title;
    g_subtitle_label = &subtitle;
    g_section_controls_label = &section_controls;
    g_section_code_label = &section_code;
    g_status_label = &status;

    Button demo_button("Sample Button", 0, 48);
    demo_button.callback(on_button_click);
    g_demo_button = &demo_button;

    Checkbox feature_checkbox("Enable advanced card", 0, 36);
    feature_checkbox.set_checked(true);
    feature_checkbox.set_toggle_callback(on_checkbox_changed);
    g_feature_checkbox = &feature_checkbox;

    Checkbox round_checkbox("Rounded corners from theme", 0, 36);
    round_checkbox.set_checked(true);
    round_checkbox.set_toggle_callback(on_checkbox_changed);
    g_round_checkbox = &round_checkbox;

    RadioButton theme_light("Light", 0, 36);
    theme_light.set_group("theme");
    theme_light.set_change_callback(on_radio_changed);
    g_theme_light = &theme_light;

    RadioButton theme_dark("Dark", 0, 36);
    theme_dark.set_group("theme");
    theme_dark.set_change_callback(on_radio_changed);
    g_theme_dark = &theme_dark;

    RadioButton theme_green("Green", 0, 36);
    theme_green.set_group("theme");
    theme_green.set_change_callback(on_radio_changed);
    g_theme_green = &theme_green;

    if (stardustui::Theme::name().equals("md3-dark")) {
        theme_dark.set_selected(true);
    } else if (stardustui::Theme::name().equals("green_light")) {
        theme_green.set_selected(true);
    } else {
        theme_light.set_selected(true);
    }

    TextBox preview_text(0, 120, true);
    preview_text.set_text("This live textbox uses the theme's textbox token set. Type here to verify borders, padding, and corner radius.");
    g_preview_text = &preview_text;

    TextBox code_box(0, 0, false);
    g_code_box = &code_box;

    controls.addComponent(title, 0);
    controls.addComponent(subtitle, 0);
    controls.addComponent(section_controls, 0);
    controls.addComponent(demo_button, 0);
    controls.addComponent(feature_checkbox, 0);
    controls.addComponent(round_checkbox, 0);
    controls.addComponent(theme_light, 0);
    controls.addComponent(theme_dark, 0);
    controls.addComponent(theme_green, 0);
    controls.addComponent(preview_text, 0);
    controls.addComponent(status, 0);

    FlexLayout code_panel(0, 0);
    code_panel.set_direction(FlexLayout::Column);
    code_panel.set_gap(14);
    code_panel.set_padding(18);
    g_code_panel_layout = &code_panel;
    code_panel.addComponent(section_code, 0);
    code_panel.addComponent(code_box, 1);

    root.addComponent(controls, 0);
    root.addComponent(code_panel, 1);

    apply_showcase_theme();
    update_showcase_code();

    window.addComponent(root);
    window.show();
    return 0;
}

#if defined(STARDUSTUI_WINDOWS) || defined(STARDUSTUI_LINUX)
int main(int argc, char *argv[], char *envp[])
{
    return showcase_main_impl(argc, argv, envp);
}
#else
extern "C" int showcase_main_cpp(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int showcase_main_cpp(int argc, char *argv[], char *envp[])
{
    return showcase_main_impl(argc, argv, envp);
}
#endif
