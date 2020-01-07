#include "gameview.hpp"

#include <fmt/format.h>

#include "dialog.hpp"
#include "file.hpp"
extern "C"
{
#include "style.h"
}

const float BUTTON_PADDING = 2.f;
const float BUTTON_Y_SPACE = FONT_Y_SPACE + BUTTON_PADDING * 2;

typedef enum
{
    InsGame,
    InsPatch,
    InsBaseComppack,
    InsPathComppack
} GameViewButton;

GameView::GameView(
        const Config* config,
        Downloader* downloader,
        DbItem* item,
        std::optional<CompPackDatabase::Item> base_comppack,
        std::optional<CompPackDatabase::Item> patch_comppack)
    : _config(config)
    , _downloader(downloader)
    , _item(item)
    , _base_comppack(base_comppack)
    , _patch_comppack(patch_comppack)
    , _patch_info_fetcher(item->titleid)
    , _image_fetcher(item)
{
    refresh();
}

void GameView::input(pkgi_input& input)
{
    start_download_package();
}

void GameView::render()
{
#define drawText(s, args...)                     \
    {                                            \
        pkgi_draw_text(                          \
                dialog_left,                     \
                cur_top,                         \
                PKGI_COLOR_TEXT_DIALOG,          \
                fmt::format(s, ##args).c_str()); \
        cur_top += FONT_Y_SPACE;                 \
    }

    float dialog_left = ALIGN_CENTER(VITA_WIDTH, PKGI_GAMEVIEW_WIDTH);
    float dialog_top = ALIGN_CENTER(VITA_HEIGHT, PKGI_GAMEVIEW_HEIGHT);

    // GameView background
    pkgi_draw_rect(
            dialog_left,
            dialog_top,
            PKGI_GAMEVIEW_WIDTH,
            BUTTON_Y_SPACE,
            PKGI_COLOR_BUTTON);
    pkgi_draw_rect(
            dialog_left,
            dialog_top + BUTTON_Y_SPACE,
            PKGI_GAMEVIEW_WIDTH,
            PKGI_GAMEVIEW_HEIGHT - FONT_Y_SPACE,
            PKGI_COLOR_GAMEVIEW_BACKGROUND);

    float cur_top = dialog_top + BUTTON_PADDING;
    dialog_left += PKGI_GAMEVIEW_PADDING;
    drawText("{} ({})", _item->name, _item->titleid);
    cur_top += PKGI_GAMEVIEW_PADDING + BUTTON_PADDING;

    auto const systemVersion = pkgi_get_system_version();
    auto const minSystemVersion = get_min_system_version();
    drawText("Firmware version: {}", systemVersion);
    drawText("Required firmware version: {}", minSystemVersion);
    cur_top += FONT_Y_SPACE;

    drawText(
            "Installed game version: {}",
            _game_version.empty() ? "not installed" : _game_version);

    if (_comppack_versions.present && _comppack_versions.base.empty() &&
        _comppack_versions.patch.empty())
    {
        drawText("Installed compatibility pack: unknown version");
    }
    else
    {
        drawText(
                "Installed base compatibility pack: {}",
                _comppack_versions.base.empty() ? "no" : "yes");
        drawText(
                "Installed patch compatibility pack version: {}",
                _comppack_versions.patch.empty() ? "none"
                                                 : _comppack_versions.patch);
    }
    cur_top += FONT_Y_SPACE;
    drawText("Diagnostic:");
    if (systemVersion < minSystemVersion)
    {
        if (!_comppack_versions.present)
        {
            if (_refood_present)
                pkgi_draw_text(
                        dialog_left,
                        cur_top,
                        PKGI_COLOR_TEXT_DIALOG,
                        "- This game will work thanks to reF00D, install the "
                        "compatibility packs for faster game boot times");
            else if (_0syscall6_present)
                pkgi_draw_text(
                        dialog_left,
                        cur_top,
                        PKGI_COLOR_TEXT_DIALOG,
                        "- This game will work thanks to 0syscall6");
            else
                pkgi_draw_text(
                        dialog_left,
                        cur_top,
                        PKGI_COLOR_TEXT_ERROR,
                        "- Your firmware is too old to play this game, you "
                        "must install the compatibility pack, reF00D, or "
                        "0syscall6");

            cur_top += FONT_Y_SPACE * 2;
        }
    }
    else
    {
        pkgi_draw_text(
                dialog_left,
                cur_top,
                PKGI_COLOR_TEXT_DIALOG,
                "- Your firmware is recent enough, no need for "
                "compatibility packs");
        cur_top += FONT_Y_SPACE * 2;
    }

    if (_comppack_versions.present && _comppack_versions.base.empty() &&
        _comppack_versions.patch.empty())
    {
        pkgi_draw_text(
                dialog_left,
                cur_top,
                PKGI_COLOR_TEXT_WARN,
                "- A compatibility pack is installed but not by PKGj, "
                "please make sure it matches the installed version or "
                "reinstall it with PKGj");
        cur_top += FONT_Y_SPACE;
    }
    cur_top += FONT_Y_SPACE;

    if (_comppack_versions.base.empty() && !_comppack_versions.patch.empty())
    {
        pkgi_draw_text(
                dialog_left,
                cur_top,
                PKGI_COLOR_TEXT_ERROR,
                "- You have installed an update compatibility pack without "
                "installing the base pack, install the base pack first and "
                "reinstall the update compatibility pack.");
        cur_top += FONT_Y_SPACE;
    }

    if (_patch_info_fetcher.get_status() == PatchInfoFetcher::Status::Found)
    {
        drawText("Install game and patch")
    }
    else
    {
        drawText("Install game");
    }

    switch (_patch_info_fetcher.get_status())
    {
    case PatchInfoFetcher::Status::Fetching:
        drawText("Checking for patch...");
        break;
    case PatchInfoFetcher::Status::NoUpdate:
        drawText("No patch found");
        break;
    case PatchInfoFetcher::Status::Found:
    {
        const auto patch_info = _patch_info_fetcher.get_patch_info();
        if (!_downloader->is_in_queue(Patch, _item->titleid))
        {
            auto button = fmt::format("Install patch {}", patch_info->version);
            drawText(button.c_str());
        }
        else
        {
            drawText("Cancel patch installation");
        }
        break;
    }
    case PatchInfoFetcher::Status::Error:
        drawText("Failed to fetch patch information");
        break;
    }

    if (_base_comppack)
    {
        if (!_downloader->is_in_queue(CompPackBase, _item->titleid))
        {
            drawText("Install base compatibility pack");
        }
        else
        {
            drawText("Cancel base compatibility pack installation");
        }
    }
    if (_patch_comppack)
    {
        if (!_downloader->is_in_queue(CompPackPatch, _item->titleid))
        {
            auto button = fmt::format(
                    "Install compatibility pack {}",
                    _patch_comppack->app_version);
            drawText(button.c_str());
        }
        else
        {
            drawText("Cancel patch compatibility pack installation");
        }
    }

    auto tex = _image_fetcher.get_texture();
    // Display game image
    if (tex != nullptr)
    {
        int tex_w = vita2d_texture_get_width(tex);
        vita2d_draw_texture(
                tex,
                dialog_top + PKGI_GAMEVIEW_PADDING + BUTTON_Y_SPACE,
                dialog_left + PKGI_GAMEVIEW_WIDTH - tex_w -
                        PKGI_GAMEVIEW_PADDING * 2);
    }
}

std::string GameView::get_min_system_version()
{
    auto const patchInfo = _patch_info_fetcher.get_patch_info();
    if (patchInfo)
        return patchInfo->fw_version;
    else
        return _item->fw_version;
}

void GameView::refresh()
{
    LOGF("refreshing gameview");
    _refood_present = pkgi_is_module_present("ref00d");
    _0syscall6_present = pkgi_is_module_present("0syscall6");
    _game_version = pkgi_get_game_version(_item->titleid);
    _comppack_versions = pkgi_get_comppack_versions(_item->titleid);
}

void GameView::start_download_package()
{
    if (_item->presence == PresenceInstalled)
    {
        LOGF("[{}] {} - already installed", _item->titleid, _item->name);
        pkgi_dialog_error("Already installed");
        return;
    }

    pkgi_start_download(*_downloader, *_item);

    _item->presence = PresenceUnknown;
}

void GameView::cancel_download_package()
{
    _downloader->remove_from_queue(Game, _item->content);
    _item->presence = PresenceUnknown;
}

void GameView::start_download_patch(const PatchInfo& patch_info)
{
    _downloader->add(DownloadItem{Patch,
                                  _item->name,
                                  _item->titleid,
                                  patch_info.url,
                                  std::vector<uint8_t>{},
                                  // TODO sha1 check
                                  std::vector<uint8_t>{},
                                  false,
                                  "ux0:",
                                  ""});
}

void GameView::cancel_download_patch()
{
    _downloader->remove_from_queue(Patch, _item->titleid);
}

void GameView::start_download_comppack(bool patch)
{
    const auto& entry = patch ? _patch_comppack : _base_comppack;

    _downloader->add(DownloadItem{patch ? CompPackPatch : CompPackBase,
                                  _item->name,
                                  _item->titleid,
                                  _config->comppack_url + entry->path,
                                  std::vector<uint8_t>{},
                                  std::vector<uint8_t>{},
                                  false,
                                  "ux0:",
                                  entry->app_version});
}

void GameView::cancel_download_comppacks(bool patch)
{
    _downloader->remove_from_queue(
            patch ? CompPackPatch : CompPackBase, _item->titleid);
}
