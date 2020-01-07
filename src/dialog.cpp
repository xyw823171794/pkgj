#include "dialog.hpp"

extern "C"
{
#include "style.h"
}
#include "pkgi.hpp"

#include <vita2d.h>

#define ALIGN_CENTER(a, b) (((a) - (b)) / 2)
namespace
{
typedef enum
{
    DialogNone,
    DialogInfo,
    DialogOK,
    DialogError,
    DialogYesNo,
} DialogType;

// Main
const float SHELL_MARGIN_X         = 20.0f;
const float SHELL_MARGIN_Y         = 18.0f;
const float FONT_Y_SPACE           = 23.0f;

DialogType dialog_type;
std::vector<std::string> dialog_text;
std::vector<Response> dialog_responses;
int dialog_width = PKGI_DIALOG_WIDTH;
}

const char* pkgi_get_ok_str(void)
{
    return pkgi_ok_button() == PKGI_BUTTON_X ? PKGI_UTF8_X : PKGI_UTF8_O;
}

const char* pkgi_get_cancel_str(void)
{
    return pkgi_cancel_button() == PKGI_BUTTON_O ? PKGI_UTF8_O : PKGI_UTF8_X;
}

void pkgi_dialog_init()
{
    dialog_type = DialogNone;
}

int pkgi_dialog_is_open()
{
    return dialog_type != DialogNone;
}

void pkgi_dialog_spit_text(const char* text) {

    size_t len = strlen(text);
    const char *str = text;

    dialog_text.clear();
    dialog_width = 0;

    for (size_t i = 0; i < len; i++)
    {
        if (text[i] == '\n')
        {
            std::string line(str, text + i - str);
            int width = pkgi_text_width(line.c_str());
            dialog_text.push_back(line);
            dialog_width = std::max(width, dialog_width);
            str = text + i + 1;            
        }
        else if (text[i] == '\0') 
        {
            int width = pkgi_text_width(str);
            dialog_text.push_back(str);
            dialog_width = std::max(width, dialog_width);
        }
    }
}

void pkgi_dialog_message(const char* text, int allow_close)
{
    pkgi_dialog_lock();
    pkgi_dialog_spit_text(text);
    dialog_type = allow_close ? DialogOK : DialogInfo;
    pkgi_dialog_unlock();
}

void pkgi_dialog_error(const char* text)
{
    LOGF("Error dialog: {}", text);
    pkgi_dialog_lock();
    pkgi_dialog_spit_text(text);
    dialog_type = DialogError;
    pkgi_dialog_unlock();
}

void pkgi_dialog_question(const std::string& text, const std::vector<Response>& responses)
{
    pkgi_dialog_lock();
    pkgi_dialog_spit_text(text.c_str());
    dialog_type = DialogYesNo;
    dialog_responses = responses;
    pkgi_dialog_unlock();
}

void pkgi_dialog_close()
{
    pkgi_dialog_lock();
    dialog_type = DialogNone;
    dialog_responses = {};
    pkgi_dialog_unlock();
}

void pkgi_do_dialog()
{
    pkgi_dialog_lock();

    DialogType local_type = dialog_type;
    auto local_text = dialog_text;
    pkgi_dialog_unlock();

    // cal bottom width
    std::string bottom_text;
    if (local_type == DialogYesNo)
    {
        bottom_text = fmt::format("{} ok {} cancel",
            pkgi_get_ok_str(), pkgi_get_cancel_str());
    }
    else if (local_type != DialogInfo)
    {
        bottom_text = fmt::format("{} close", pkgi_get_ok_str());
    }
    int bottom_width = pkgi_text_width(bottom_text.c_str());

    // dialog position
    int local_width = std::max(dialog_width, bottom_width) + SHELL_MARGIN_X * 2;
    int local_height = local_text.size() * FONT_Y_SPACE + SHELL_MARGIN_Y * 2;
    if (local_type != DialogInfo) local_height += FONT_Y_SPACE * 2;

    float dialog_left = ALIGN_CENTER(VITA_WIDTH, local_width);
    float dialog_top = ALIGN_CENTER(VITA_HEIGHT, local_height);
    // Dialog background
    pkgi_draw_rect(dialog_left, dialog_top, local_width, 
        local_height, PKGI_COLOR_DIALOG_BACKGROUND);

    dialog_top += SHELL_MARGIN_Y;
    for (size_t i = 0; i < local_text.size(); i++)
    {
        pkgi_draw_text(dialog_left + SHELL_MARGIN_X, dialog_top, 
                PKGI_COLOR_TEXT_DIALOG, local_text[i].c_str());
        dialog_top += FONT_Y_SPACE;
    }
    if (local_type != DialogInfo)
    {
        pkgi_draw_text(ALIGN_CENTER(VITA_WIDTH, bottom_width), 
            dialog_top + FONT_Y_SPACE, PKGI_COLOR_TEXT_DIALOG, bottom_text.c_str());
    }
}
