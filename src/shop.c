#include "global.h"
#include "bg.h"
#include "data.h"
#include "decompress.h"
#include "decoration.h"
#include "decoration_inventory.h"
#include "event_object_movement.h"
#include "field_player_avatar.h"
#include "field_screen_effect.h"
#include "field_specials.h"
#include "field_weather.h"
#include "fieldmap.h"
#include "gpu_regs.h"
#include "graphics.h"
#include "international_string_util.h"
#include "item.h"
#include "item_icon.h"
#include "item_menu.h"
#include "item_menu_icons.h"
#include "list_menu.h"
#include "main.h"
#include "malloc.h"
#include "menu.h"
#include "menu_helpers.h"
#include "money.h"
#include "overworld.h"
#include "palette.h"
#include "party_menu.h"
#include "scanline_effect.h"
#include "script.h"
#include "shop.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "strings.h"
#include "text_window.h"
#include "tv.h"
#include "constants/decorations.h"
#include "constants/items.h"
#include "constants/metatile_behaviors.h"
#include "constants/rgb.h"
#include "constants/songs.h"
#include "constants/tv.h"

EWRAM_DATA struct MartInfo gMartInfo = {0};
EWRAM_DATA struct ShopData *gShopDataPtr = NULL;
EWRAM_DATA struct ListMenuItem *sShopMenuListMenu = NULL;
EWRAM_DATA u8 (*sShopMenuItemStrings)[16] = {0};
EWRAM_DATA u8 gMartPurchaseHistoryId = 0;
EWRAM_DATA struct ItemSlot gMartPurchaseHistory[3] = {0};

static void Task_ShopMenu(u8 taskId);
static void Task_HandleShopMenuQuit(u8 taskId);
static void CB2_InitBuyMenu(void);
static void Task_GoToBuyOrSellMenu(u8 taskId);
static void MapPostLoadHook_ReturnToShopMenu(void);
static void Task_ReturnToShopMenu(u8 taskId);
static void ShowShopMenuAfterExitingBuyOrSellMenu(u8 taskId);
static void BuyMenuDrawGraphics(void);
static void BuyMenuAddScrollIndicatorArrows(void);
static void Task_BuyMenu(u8 taskId);
static bool8 BuyMenuBuildListMenuTemplate(void);
static void BuyMenuInitBgs(void);
static void BuyMenuInitWindows(u8 martType);
static void BuyMenuDecompressBgGraphics(void);
static void BuyMenuSetListEntry(struct ListMenuItem*, u16, u8*);
static void BuyMenuAddItemIcon(u16, u8);
static void BuyMenuRemoveItemIcon(u16, u8);
static void BuyMenuPrint(u8 windowId, u8 font, const u8 *text, u8 x, u8 y, u8 letterSpacing, u8 lineSpacing, s8 speed, u8 colorSet);
static void BuyMenuDrawMapGraphics(void);
static void BuyMenuCopyMenuBgToBg1TilemapBuffer(void);
static void BuyMenuCollectObjectEventData(void);
static void BuyMenuDrawObjectEvents(void);
static void BuyMenuDrawMapBg(void);
static void BuyMenuDrawMapMetatile(s16, s16, const u16*, u8);
static void BuyMenuDrawMapMetatileLayer(u16 *dest, s16 offset1, s16 offset2, const u16 *src);
static void ExitBuyMenu(u8 taskId);
static void Task_ExitBuyMenu(u8 taskId);
static void BuyMenuTryMakePurchase(u8 taskId);
static void BuyMenuReturnToItemList(u8 taskId);
static void Task_BuyHowManyDialogueInit(u8 taskId);
static void BuyMenuConfirmPurchase(u8 taskId);
static void BuyMenuPrintItemQuantityAndPrice(u8 taskId);
static void Task_BuyHowManyDialogueHandleInput(u8 taskId);
static void BuyMenuSubtractMoney(u8 taskId);
static void RecordItemPurchase(u8 taskId);
static void Task_ReturnToItemListAfterItemPurchase(u8 taskId);
static void Task_ReturnToItemListAfterDecorationPurchase(u8 taskId);
static void Task_HandleShopMenuBuy(u8 taskId);
static void Task_HandleShopMenuSell(u8 taskId);
static void BuyMenuPrintItemDescriptionAndShowItemIcon(s32 item, bool8 onInit, struct ListMenu *list);
static void BuyMenuPrintPriceInList(u8 windowId, s32 item, u8 y);
static u8 GetMartTypeFromItemList(u32 martType);
static bool8 InitShopData(void);
static void BuyMenuFreeMemory(void);
static void BuyMenuQuantityBoxNormalBorder(u8 windowId, bool8 copyToVram);
static void BuyMenuQuantityBoxThinBorder(u8 windowId, bool8 copyToVram);

static const struct YesNoFuncTable sShopPurchaseYesNoFuncs =
{
    BuyMenuTryMakePurchase,
    BuyMenuReturnToItemList
};

static const struct MenuAction sShopMenuActions_BuySellQuit[] =
{
    { gText_ShopBuy, {.void_u8=Task_HandleShopMenuBuy} },
    { gText_ShopSell, {.void_u8=Task_HandleShopMenuSell} },
    { gText_ShopQuit, {.void_u8=Task_HandleShopMenuQuit} }
};

static const struct MenuAction sShopMenuActions_BuyQuit[] =
{
    { gText_ShopBuy, {.void_u8=Task_HandleShopMenuBuy} },
    { gText_ShopQuit, {.void_u8=Task_HandleShopMenuQuit} }
};

static const struct WindowTemplate sShopMenuWindowTemplates[] =
{
    {
        .bg = 0,
        .tilemapLeft = 2,
        .tilemapTop = 1,
        .width = 9,
        .height = 6,
        .paletteNum = 15,
        .baseBlock = 0x0008,
    },
    {
        .bg = 0,
        .tilemapLeft = 2,
        .tilemapTop = 1,
        .width = 12,
        .height = 4,
        .paletteNum = 15,
        .baseBlock = 0x0008,
    }
};

static const struct ListMenuTemplate sShopBuyMenuListTemplate =
{
    .items = NULL,
    .moveCursorFunc = BuyMenuPrintItemDescriptionAndShowItemIcon,
    .itemPrintFunc = BuyMenuPrintPriceInList,
    .totalItems = 0,
    .maxShowed = 0,
    .windowId = 4,
    .header_X = 0,
    .item_X = 8,
    .cursor_X = 0,
    .upText_Y = 1,
    .cursorPal = 2,
    .fillValue = 0,
    .cursorShadowPal = 3,
    .lettersSpacing = 0,
    .itemVerticalPadding = 0,
    .scrollMultiple = LIST_NO_MULTIPLE_SCROLL,
    .fontId = 7,
    .cursorKind = 0
};

static const struct BgTemplate sShopBuyMenuBgTemplates[] =
{
    {
        .bg = 0,
        .charBaseIndex = 2,
        .mapBaseIndex = 31,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 0,
        .baseTile = 0
    },
    {
        .bg = 1,
        .charBaseIndex = 0,
        .mapBaseIndex = 30,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 1,
        .baseTile = 0
    },
    {
        .bg = 2,
        .charBaseIndex = 0,
        .mapBaseIndex = 29,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 2,
        .baseTile = 0
    },
    {
        .bg = 3,
        .charBaseIndex = 0,
        .mapBaseIndex = 28,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 3,
        .baseTile = 0
    }
};

static const struct WindowTemplate sShopBuyMenuWindowTemplates[] =
{
    {
        .bg = 0x0,
        .tilemapLeft = 0x1,
        .tilemapTop = 0x1,
        .width = 0x8,
        .height = 0x3,
        .paletteNum = 0xF,
        .baseBlock = 0x27,
    },
    {
        .bg = 0x0,
        .tilemapLeft = 0x1,
        .tilemapTop = 0xB,
        .width = 0xD,
        .height = 0x2,
        .paletteNum = 0xF,
        .baseBlock = 0x3F,
    },
    {
        .bg = 0x0,
        .tilemapLeft = 0x2,
        .tilemapTop = 0xF,
        .width = 0x1A,
        .height = 0x4,
        .paletteNum = 0xE,
        .baseBlock = 0x59,
    },
    {
        .bg = 0x0,
        .tilemapLeft = 0x11,
        .tilemapTop = 0x9,
        .width = 0xC,
        .height = 0x4,
        .paletteNum = 0xE,
        .baseBlock = 0xC1,
    },
    {
        .bg = 0x0,
        .tilemapLeft = 0xB,
        .tilemapTop = 0x1,
        .width = 0x11,
        .height = 0xC,
        .paletteNum = 0xE,
        .baseBlock = 0xF1,
    },
    {
        .bg = 0x0,
        .tilemapLeft = 0x5,
        .tilemapTop = 0xE,
        .width = 0x19,
        .height = 0x6,
        .paletteNum = 0xF,
        .baseBlock = 0x1BD,
    },
    DUMMY_WIN_TEMPLATE
};

static const struct WindowTemplate sShopBuyMenuYesNoWindowTemplates =
{
    .bg = 0,
    .tilemapLeft = 21,
    .tilemapTop = 9,
    .width = 6,
    .height = 4,
    .paletteNum = 14,
    .baseBlock = 0xC1,
};

static const u8 sShopBuyMenuTextColors[][3] =
{
    {0, 1, 2},
    {0, 2, 3},
    {0, 3, 2}
};

static u8 CreateShopMenu(u8 martType)
{
    int numMenuItems;

    ScriptContext2_Enable();
    gMartInfo.martType = GetMartTypeFromItemList(martType);

    if (martType == MART_TYPE_NORMAL)
    {
        struct WindowTemplate winTemplate;
        winTemplate = sShopMenuWindowTemplates[0];
        //winTemplate.width = GetMaxWidthInMenuTable(sShopMenuActions_BuySellQuit, ARRAY_COUNT(sShopMenuActions_BuySellQuit));
        gMartInfo.windowId = AddWindow(&winTemplate);
        gMartInfo.menuActions = sShopMenuActions_BuySellQuit;
        numMenuItems = ARRAY_COUNT(sShopMenuActions_BuySellQuit);
    }
    else
    {
        struct WindowTemplate winTemplate;
        winTemplate = sShopMenuWindowTemplates[1];
        winTemplate.width = GetMaxWidthInMenuTable(sShopMenuActions_BuyQuit, ARRAY_COUNT(sShopMenuActions_BuyQuit));
        gMartInfo.windowId = AddWindow(&winTemplate);
        gMartInfo.menuActions = sShopMenuActions_BuyQuit;
        numMenuItems = ARRAY_COUNT(sShopMenuActions_BuyQuit);
    }

    SetStandardWindowBorderStyle(gMartInfo.windowId, 0);
    PrintMenuTable(gMartInfo.windowId, numMenuItems, gMartInfo.menuActions);
    InitMenuInUpperLeftCornerPlaySoundWhenAPressed(gMartInfo.windowId, 1, 0, 2, 16, numMenuItems, 0);
    PutWindowTilemap(gMartInfo.windowId);
    CopyWindowToVram(gMartInfo.windowId, 1);

    return CreateTask(Task_ShopMenu, 8);
}

static u8 GetMartTypeFromItemList(u32 martType)
{    
    u16 i;
    
    if (martType != MART_TYPE_NORMAL)
        return martType;
    
    for (i = 0; i < gMartInfo.itemCount && gMartInfo.itemList[i] != 0; i++)
    {
        if (ItemId_GetPocket(gMartInfo.itemList[i]) == POCKET_TM_HM)
            return MART_TYPE_TMHM;
    }
    return MART_TYPE_NORMAL;
}

static void SetShopMenuCallback(void (* callback)(void))
{
    gMartInfo.callback = callback;
}

static void SetShopItemsForSale(const u16 *items)
{
    gMartInfo.itemList = items;
    gMartInfo.itemCount = 0;

    while (gMartInfo.itemList[gMartInfo.itemCount])
    {
        gMartInfo.itemCount++;
    }
}

static void Task_ShopMenu(u8 taskId)
{
    s8 inputCode = Menu_ProcessInputNoWrap();
    switch (inputCode)
    {
    case MENU_NOTHING_CHOSEN:
        break;
    case MENU_B_PRESSED:
        PlaySE(SE_SELECT);
        Task_HandleShopMenuQuit(taskId);
        break;
    default:
        gMartInfo.menuActions[inputCode].func.void_u8(taskId);
        break;
    }
}

static void Task_HandleShopMenuBuy(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    data[8] = (u32)CB2_InitBuyMenu >> 16;
    data[9] = (u32)CB2_InitBuyMenu;
    gTasks[taskId].func = Task_GoToBuyOrSellMenu;
    FadeScreen(FADE_TO_BLACK, 0);
}

static void Task_HandleShopMenuSell(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    data[8] = (u32)CB2_GoToSellMenu >> 16;
    data[9] = (u32)CB2_GoToSellMenu;
    gTasks[taskId].func = Task_GoToBuyOrSellMenu;
    FadeScreen(FADE_TO_BLACK, 0);
}

void CB2_ExitSellMenu(void)
{
    gFieldCallback = MapPostLoadHook_ReturnToShopMenu;
    SetMainCallback2(CB2_ReturnToField);
}

static void Task_HandleShopMenuQuit(u8 taskId)
{
    ClearStdWindowAndFrameToTransparent(gMartInfo.windowId, 2);
    RemoveWindow(gMartInfo.windowId);
    SaveRecordedItemPurchasesForTVShow();
    ScriptContext2_Disable();
    DestroyTask(taskId);

    if (gMartInfo.callback)
        gMartInfo.callback();
}

static void Task_GoToBuyOrSellMenu(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    if (!gPaletteFade.active)
    {
        DestroyTask(taskId);
        SetMainCallback2((void *)((u16)data[8] << 16 | (u16)data[9]));
    }
}

static void MapPostLoadHook_ReturnToShopMenu(void)
{
    FadeInFromBlack();
    CreateTask(Task_ReturnToShopMenu, 8);
}

static void Task_ReturnToShopMenu(u8 taskId)
{
    if (IsWeatherNotFadingIn() == TRUE)
    {
        if (gMartInfo.martType == MART_TYPE_DECOR2)
            DisplayItemMessageOnField(taskId, gText_CanIHelpWithAnythingElse, ShowShopMenuAfterExitingBuyOrSellMenu);
        else
            DisplayItemMessageOnField(taskId, gText_AnythingElseICanHelp, ShowShopMenuAfterExitingBuyOrSellMenu);
    }
}

static void ShowShopMenuAfterExitingBuyOrSellMenu(u8 taskId)
{
    CreateShopMenu(gMartInfo.martType);
    DestroyTask(taskId);
}

static void CB2_BuyMenu(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    do_scheduled_bg_tilemap_copies_to_vram();
    UpdatePaletteFade();
}

static void VBlankCB_BuyMenu(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

#define tItemCount data[1]
#define tItemId data[5]
#define tListTaskId data[7]

static void CB2_InitBuyMenu(void)
{
    u8 taskId;

    switch (gMain.state)
    {
    case 0:
        SetVBlankHBlankCallbacksToNull();
        CpuFastFill(0, (void *)OAM, OAM_SIZE);
        ScanlineEffect_Stop();
        reset_temp_tile_data_buffers();
        FreeAllSpritePalettes();
        ResetPaletteFade();
        ResetSpriteData();
        ResetTasks();
        clear_scheduled_bg_copies_to_vram();
        ResetItemMenuIconState();

        if (!InitShopData() || !BuyMenuBuildListMenuTemplate())
            return;
        BuyMenuInitBgs();
        FillBgTilemapBufferRect_Palette0(0, 0, 0, 0, 0x20, 0x20);
        FillBgTilemapBufferRect_Palette0(1, 0, 0, 0, 0x20, 0x20);
        FillBgTilemapBufferRect_Palette0(2, 0, 0, 0, 0x20, 0x20);
        FillBgTilemapBufferRect_Palette0(3, 0, 0, 0, 0x20, 0x20);
        BuyMenuInitWindows(gMartInfo.martType);
        BuyMenuDecompressBgGraphics();
        gMain.state++;
        break;
    case 1:
        if (!free_temp_tile_data_buffers_if_possible())
            gMain.state++;
        break;
    default:
        gShopDataPtr->selectedRow = 0;
        gShopDataPtr->scrollOffset = 0;
        BuyMenuDrawGraphics();
        BuyMenuAddScrollIndicatorArrows();
        taskId = CreateTask(Task_BuyMenu, 8);
        gTasks[taskId].tListTaskId = ListMenuInit(&gMultiuseListMenuTemplate, 0, 0);
        BlendPalettes(0xFFFFFFFF, 0x10, RGB_BLACK);
        BeginNormalPaletteFade(0xFFFFFFFF, 0, 0x10, 0, RGB_BLACK);
        SetVBlankCallback(VBlankCB_BuyMenu);
        SetMainCallback2(CB2_BuyMenu);
        break;
    }
}

static bool8 InitShopData(void)
{
    gShopDataPtr = AllocZeroed(sizeof(struct ShopData));
    if (gShopDataPtr == NULL)
    {
        BuyMenuFreeMemory();
        CB2_ExitSellMenu();
        return FALSE;
    }

    gShopDataPtr->scrollIndicatorsTaskId = 0xFF;
    gShopDataPtr->itemSpriteIds[0] = 0xFF;
    gShopDataPtr->itemSpriteIds[1] = 0xFF;

    return TRUE;
}

static void BuyMenuFreeMemory(void)
{
    Free(gShopDataPtr);
    Free(sShopMenuListMenu);
    Free(sShopMenuItemStrings);
    FreeAllWindowBuffers();
}

static bool8 BuyMenuBuildListMenuTemplate(void)
{
    u16 i, max;
    u16 itemCount;

    sShopMenuListMenu = Alloc((gMartInfo.itemCount + 1) * sizeof(*sShopMenuListMenu));
    if (sShopMenuListMenu == NULL
     || (sShopMenuItemStrings = Alloc((gMartInfo.itemCount + 1) * sizeof(*sShopMenuItemStrings))) == NULL)
    {
        BuyMenuFreeMemory();
        CB2_ExitSellMenu();
        return FALSE;
    }
    
    for (i = 0; i < gMartInfo.itemCount; i++)
        BuyMenuSetListEntry(&sShopMenuListMenu[i], gMartInfo.itemList[i], sShopMenuItemStrings[i]);

    StringCopy(sShopMenuItemStrings[i], gText_Cancel2);
    sShopMenuListMenu[i].name = sShopMenuItemStrings[i];
    sShopMenuListMenu[i].id = -2;

    gMultiuseListMenuTemplate.items = sShopMenuListMenu;
    gMultiuseListMenuTemplate.totalItems = gMartInfo.itemCount + 1;
    gMultiuseListMenuTemplate.windowId = 4;
    gMultiuseListMenuTemplate.header_X = 0;
    gMultiuseListMenuTemplate.item_X = 9;
    gMultiuseListMenuTemplate.cursor_X = 1;
    gMultiuseListMenuTemplate.lettersSpacing = 0;
    gMultiuseListMenuTemplate.itemVerticalPadding = 2;
    gMultiuseListMenuTemplate.upText_Y = 2;
    gMultiuseListMenuTemplate.fontId = 1;
    gMultiuseListMenuTemplate.fillValue = 0;
    gMultiuseListMenuTemplate.cursorPal = GetFontAttribute(2, FONTATTR_COLOR_FOREGROUND);
    gMultiuseListMenuTemplate.cursorShadowPal = GetFontAttribute(2, FONTATTR_COLOR_SHADOW);
    gMultiuseListMenuTemplate.moveCursorFunc = BuyMenuPrintItemDescriptionAndShowItemIcon;
    gMultiuseListMenuTemplate.itemPrintFunc = BuyMenuPrintPriceInList;
    gMultiuseListMenuTemplate.scrollMultiple = 0;
    gMultiuseListMenuTemplate.cursorKind = 0;
    
    if (gMartInfo.martType == MART_TYPE_TMHM)
        max = 5;
    else
        max = 6;
    
    if (gMultiuseListMenuTemplate.totalItems > max)
        gMultiuseListMenuTemplate.maxShowed = max;
    else
        gMultiuseListMenuTemplate.maxShowed = gMultiuseListMenuTemplate.totalItems;

    gShopDataPtr->itemsShowed = gMultiuseListMenuTemplate.maxShowed;
    return TRUE;
}

static void BuyMenuSetListEntry(struct ListMenuItem *menuItem, u16 item, u8 *name)
{
    if (gMartInfo.martType < MART_TYPE_DECOR)
        CopyItemName(item, name);
    else
        StringCopy(name, gDecorations[item].name);

    menuItem->name = name;
    menuItem->id = item;
}

static void BuyMenuPrintItemDescriptionAndShowItemIcon(s32 item, bool8 onInit, struct ListMenu *list)
{
    const u8 *description;

    if (onInit != TRUE)
        PlaySE(SE_SELECT);

    if (item != LIST_CANCEL)
    {
        if (gMartInfo.martType < MART_TYPE_DECOR)
            description = ItemId_GetDescription(item);
        else
            description = gDecorations[item].description;
    }
    else
    {
        description = gText_QuitShopping;
    }

    FillWindowPixelBuffer(5, PIXEL_FILL(0));
    if (gMartInfo.martType != MART_TYPE_TMHM)
    {
        BuyMenuRemoveItemIcon(item, gShopDataPtr->iconSlot ^ 1);
        if (item != LIST_CANCEL)
            BuyMenuAddItemIcon(item, gShopDataPtr->iconSlot);
        else
            BuyMenuAddItemIcon(-1, gShopDataPtr->iconSlot);

        gShopDataPtr->iconSlot ^= 1;
        BuyMenuPrint(5, 1, description, 0, 3, 2, 1, 0, 0);
    }
    else
    {
        FillWindowPixelBuffer(6, PIXEL_FILL(0));
        //LoadTmHmNameInMart(item);
        BuyMenuPrint(5, 1, description, 2, 3, 1, 0, 0, 0);
    }
}

static void BuyMenuPrintPriceInList(u8 windowId, s32 item, u8 y)
{
    s32 x;
    u8 *loc;

    if (item != LIST_CANCEL)
    {
        if (gMartInfo.martType < MART_TYPE_DECOR)
        {
            ConvertIntToDecimalStringN(
                gStringVar1,
                ItemId_GetPrice(item) >> GetPriceReduction(POKENEWS_SLATEPORT),
                STR_CONV_MODE_LEFT_ALIGN,
                5);
        }
        else
        {
            ConvertIntToDecimalStringN(
                gStringVar1,
                gDecorations[item].price,
                STR_CONV_MODE_LEFT_ALIGN,
                5);
        }
        
        x = 4 - StringLength(gStringVar1);
        loc = gStringVar4;
        while (x-- != 0)
            *loc++ = 0;
        StringExpandPlaceholders(loc, gText_PokedollarVar1);
        BuyMenuPrint(windowId, 0, gStringVar4, 0x69, y, 0, 0, TEXT_SPEED_FF, 1);
    }
}

static void BuyMenuAddScrollIndicatorArrows(void)
{
    if (gMartInfo.martType != MART_TYPE_TMHM)
    {
        gShopDataPtr->scrollIndicatorsTaskId = AddScrollIndicatorArrowPairParameterized(SCROLL_ARROW_UP, 160, 8, 104, 
            (gMartInfo.itemCount - gShopDataPtr->itemsShowed) + 1, 110, 110, &gShopDataPtr->scrollOffset);
    }
    else
    {
        gShopDataPtr->scrollIndicatorsTaskId = AddScrollIndicatorArrowPairParameterized(SCROLL_ARROW_UP, 160, 8, 88, 
            (gMartInfo.itemCount - gShopDataPtr->itemsShowed) + 1, 110, 110, &gShopDataPtr->scrollOffset);
    }
}

static void BuyQuantityAddScrollIndicatorArrows(void)
{
    gShopDataPtr->buyMenuArrowScrollOffset = 1;
    gShopDataPtr->scrollIndicatorsTaskId = AddScrollIndicatorArrowPairParameterized(SCROLL_ARROW_UP, 152, 72, 104, 2, 0x6E, 0x6E, &gShopDataPtr->buyMenuArrowScrollOffset);
}

static void BuyMenuRemoveScrollIndicatorArrows(void)
{
    if (gShopDataPtr->scrollIndicatorsTaskId != 0xFF)
    {
        RemoveScrollIndicatorArrowPair(gShopDataPtr->scrollIndicatorsTaskId);
        gShopDataPtr->scrollIndicatorsTaskId = 0xFF;
    }
}

static void BuyMenuPrintCursor(u8 scrollIndicatorsTaskId, u8 colorSet)
{
    u8 y = ListMenuGetYCoordForPrintingArrowCursor(scrollIndicatorsTaskId);
    BuyMenuPrint(4, 1, gText_SelectorArrow2, 1, y, 0, 0, 0, colorSet);
}

static void BuyMenuAddItemIcon(u16 item, u8 iconSlot)
{
    u8 spriteId;
    u8 *spriteIdPtr = &gShopDataPtr->itemSpriteIds[iconSlot];
    if (*spriteIdPtr != 0xFF)
        return;

    if (gMartInfo.martType == MART_TYPE_NORMAL || item == 0xFFFF)
    {
        FreeSpriteTilesByTag(102 + iconSlot);
        FreeSpritePaletteByTag(102 + iconSlot);
        spriteId = AddItemIconSprite(iconSlot + 102, iconSlot + 102, item);
        if (spriteId != MAX_SPRITES)
        {
            *spriteIdPtr = spriteId;
            gSprites[spriteId].pos2.x = 24;
            gSprites[spriteId].pos2.y = 140;
        }
    }
    else
    {
        spriteId = AddDecorationIconObject(item, 20, 84, 1, iconSlot + 2110, iconSlot + 2110);
        if (spriteId != MAX_SPRITES)
            *spriteIdPtr = spriteId;
    }
}

static void BuyMenuRemoveItemIcon(u16 item, u8 iconSlot)
{
    u8 *spriteIdPtr = &gShopDataPtr->itemSpriteIds[iconSlot];
    if (*spriteIdPtr == 0xFF)
        return;

    FreeSpriteTilesByTag(iconSlot + 2110);
    FreeSpritePaletteByTag(iconSlot + 2110);
    DestroySprite(&gSprites[*spriteIdPtr]);
    *spriteIdPtr = 0xFF;
}

static void BuyMenuInitBgs(void)
{
    ResetBgsAndClearDma3BusyFlags(0);
    InitBgsFromTemplates(0, sShopBuyMenuBgTemplates, ARRAY_COUNT(sShopBuyMenuBgTemplates));
    SetBgTilemapBuffer(1, gShopDataPtr->tilemapBuffers[1]);
    SetBgTilemapBuffer(2, gShopDataPtr->tilemapBuffers[3]);
    SetBgTilemapBuffer(3, gShopDataPtr->tilemapBuffers[2]);
    SetGpuReg(REG_OFFSET_BG0HOFS, 0);
    SetGpuReg(REG_OFFSET_BG0VOFS, 0);
    SetGpuReg(REG_OFFSET_BG1HOFS, 0);
    SetGpuReg(REG_OFFSET_BG1VOFS, 0);
    SetGpuReg(REG_OFFSET_BG2HOFS, 0);
    SetGpuReg(REG_OFFSET_BG2VOFS, 0);
    SetGpuReg(REG_OFFSET_BG3HOFS, 0);
    SetGpuReg(REG_OFFSET_BG3VOFS, 0);
    SetGpuReg(REG_OFFSET_BLDCNT, 0);
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0 | DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
    ShowBg(0);
    ShowBg(1);
    ShowBg(2);
    ShowBg(3);
}

static void BuyMenuDecompressBgGraphics(void)
{
    void* pal;
    
    decompress_and_copy_tile_data_to_vram(1, gBuyMenuFrame_Gfx, 0x480, 0x3DC, 0);
    if ((gMartInfo.martType) != MART_TYPE_TMHM)
        LZDecompressWram(gBuyMenuFrame_Tilemap, gShopDataPtr->tilemapBuffers[0]);
    else
        LZDecompressWram(gBuyMenuFrame_TmHmTilemap, gShopDataPtr->tilemapBuffers[0]);

    pal = Alloc(0x40);
    LZDecompressWram(gMenuMoneyPal, pal);
    LoadPalette(pal, 0xB0, 0x20);
    LoadPalette(pal + 0x20, 0x60, 0x20);
    Free(pal);
}

static void sub_809B10C(bool32 a0)
{
    u8 v;
    
    if (a0 == FALSE)
        v = 0xB;
    else 
        v = 6;
    
    if ((gMartInfo.martType) != MART_TYPE_TMHM)
        SetBgTilemapPalette(1, 0, 0xE, 0x1E, 6, v);
    else
        SetBgTilemapPalette(1, 0, 0xC, 0x1E, 8, v);
    
    schedule_bg_copy_tilemap_to_vram(1);
}

static void BuyMenuInitWindows(u8 martType)
{
    if (martType != MART_TYPE_TMHM)
    {
        InitWindows(sShopBuyMenuWindowTemplates);
    }
    else
    {
        // TODO
        InitWindows(sShopBuyMenuWindowTemplates);
    }

    DeactivateAllTextPrinters();
    LoadUserWindowBorderGfx(0, 1, 0xD0);
    LoadMessageBoxGfx(0, 0x13, 0xE0);
    LoadThinWindowBorderGfx(0, 0xA, 0xF0);
    PutWindowTilemap(0);
    PutWindowTilemap(4);
    PutWindowTilemap(5);

    if (martType == MART_TYPE_TMHM)
        PutWindowTilemap(6);
}

static void BuyMenuPrint(u8 windowId, u8 font, const u8 *text, u8 x, u8 y, u8 letterSpacing, u8 lineSpacing, s8 speed, u8 colorSet)
{
    AddTextPrinterParameterized4(windowId, font, x, y, letterSpacing, lineSpacing, sShopBuyMenuTextColors[colorSet], speed, text);
}

static void BuyMenuDisplayMessage(u8 taskId, const u8 *text, TaskFunc callback)
{
    DisplayMessageAndContinueTask(taskId, 2, 19, 14, 1, GetPlayerTextSpeedDelay(), text, callback);
    schedule_bg_copy_tilemap_to_vram(0);
}

static void BuyMenuQuantityBoxNormalBorder(u8 windowId, bool8 copyToVram)
{
    DrawStdFrameWithCustomTileAndPalette(windowId, copyToVram, 0x1, 0xD);
}

static void BuyMenuQuantityBoxThinBorder(u8 windowId, bool8 copyToVram)
{
    DrawStdFrameWithCustomTileAndPalette(windowId, copyToVram, 0xA, 0xF);
}

static void BuyMenuDrawGraphics(void)
{
    BuyMenuDrawMapGraphics();
    BuyMenuCopyMenuBgToBg1TilemapBuffer();
    //AddMoneyLabelObject(19, 11);
    PrintMoneyAmountInMoneyBoxWithBorder(0, 10, 15, GetMoney(&gSaveBlock1Ptr->money));
    schedule_bg_copy_tilemap_to_vram(0);
    schedule_bg_copy_tilemap_to_vram(1);
    schedule_bg_copy_tilemap_to_vram(2);
    schedule_bg_copy_tilemap_to_vram(3);
}

static void BuyMenuDrawMapGraphics(void)
{
    BuyMenuCollectObjectEventData();
    BuyMenuDrawObjectEvents();
    BuyMenuDrawMapBg();
}

static void BuyMenuDrawMapBg(void)
{
    s16 i, j, x, y;
    const struct MapLayout *mapLayout;
    u16 metatile;
    u8 metatileLayerType;

    mapLayout = gMapHeader.mapLayout;
    GetXYCoordsOneStepInFrontOfPlayer(&x, &y);
    x -= 2;
    y -= 3;

    for (j = 0; j < 10; j++)
    {
        for (i = 0; i < 5; i++)
        {
            metatile = MapGridGetMetatileIdAt(x + i, y + j);
            metatileLayerType = MapGridGetMetatileLayerTypeAt(x + i, y + j);

            if (metatile < NUM_METATILES_IN_PRIMARY)
            {
                BuyMenuDrawMapMetatile(i, j, (u16*)mapLayout->primaryTileset->metatiles + metatile * 8, metatileLayerType);
            }
            else
            {
                BuyMenuDrawMapMetatile(i, j, (u16*)mapLayout->secondaryTileset->metatiles + ((metatile - NUM_METATILES_IN_PRIMARY) * 8), metatileLayerType);
            }
        }
    }
}

static void BuyMenuDrawMapMetatile(s16 x, s16 y, const u16 *src, u8 metatileLayerType)
{
    u16 offset1 = x * 2;
    u16 offset2 = y * 64 + 64;

    switch (metatileLayerType)
    {
    case 0:
        BuyMenuDrawMapMetatileLayer(gShopDataPtr->tilemapBuffers[3], offset1, offset2, src);
        BuyMenuDrawMapMetatileLayer(gShopDataPtr->tilemapBuffers[1], offset1, offset2, src + 4);
        break;
    case 1:
        BuyMenuDrawMapMetatileLayer(gShopDataPtr->tilemapBuffers[2], offset1, offset2, src);
        BuyMenuDrawMapMetatileLayer(gShopDataPtr->tilemapBuffers[3], offset1, offset2, src + 4);
        break;
    case 2:
        BuyMenuDrawMapMetatileLayer(gShopDataPtr->tilemapBuffers[2], offset1, offset2, src);
        BuyMenuDrawMapMetatileLayer(gShopDataPtr->tilemapBuffers[1], offset1, offset2, src + 4);
        break;
    }
}

static void BuyMenuDrawMapMetatileLayer(u16 *dest, s16 offset1, s16 offset2, const u16 *src)
{
    // This function draws a whole 2x2 metatile.
    dest[offset1 + offset2] = src[0]; // top left
    dest[offset1 + offset2 + 1] = src[1]; // top right
    dest[offset1 + offset2 + 32] = src[2]; // bottom left
    dest[offset1 + offset2 + 33] = src[3]; // bottom right
}

static void BuyMenuCollectObjectEventData(void)
{
    s16 facingX, facingY;
    u8 x, y, z;
    u8 num = 0;

    GetXYCoordsOneStepInFrontOfPlayer(&facingX, &facingY);
    z = PlayerGetZCoord();

    for (y = 0; y < OBJECT_EVENTS_COUNT; y++)
        gShopDataPtr->viewportObjects[y][OBJ_EVENT_ID] = 16;
    
    for (y = 0; y < 5; y++)
    {
        for (x = 0; x < 7; x++)
        {
            u8 objEventId = GetObjectEventIdByXYZ(facingX - 3 + x, facingY - 2 + y, z);
            if (objEventId != OBJECT_EVENTS_COUNT)
            {
                gShopDataPtr->viewportObjects[num][OBJ_EVENT_ID] = objEventId;
                gShopDataPtr->viewportObjects[num][X_COORD] = x;
                gShopDataPtr->viewportObjects[num][Y_COORD] = y;

                switch (gObjectEvents[objEventId].facingDirection)
                {
                    case DIR_SOUTH:
                        gShopDataPtr->viewportObjects[num][ANIM_NUM] = 0;
                        break;
                    case DIR_NORTH:
                        gShopDataPtr->viewportObjects[num][ANIM_NUM] = 1;
                        break;
                    case DIR_WEST:
                        gShopDataPtr->viewportObjects[num][ANIM_NUM] = 2;
                        break;
                    case DIR_EAST:
                    default:
                        gShopDataPtr->viewportObjects[num][ANIM_NUM] = 3;
                        break;
                }
                num++;
            }
        }
    }
}

static void BuyMenuDrawObjectEvents(void)
{
    u8 i;
    u8 spriteId;
    const struct ObjectEventGraphicsInfo *graphicsInfo;

    for (i = 0; i < OBJECT_EVENTS_COUNT; i++) // max objects?
    {
        if (gShopDataPtr->viewportObjects[i][OBJ_EVENT_ID] == OBJECT_EVENTS_COUNT)
            continue;

        graphicsInfo = GetObjectEventGraphicsInfo(gObjectEvents[gShopDataPtr->viewportObjects[i][OBJ_EVENT_ID]].graphicsId);

        spriteId = AddPseudoObjectEvent(
            gObjectEvents[gShopDataPtr->viewportObjects[i][OBJ_EVENT_ID]].graphicsId,
            SpriteCallbackDummy,
            (u16)gShopDataPtr->viewportObjects[i][X_COORD] * 16 - 8,
            (u16)gShopDataPtr->viewportObjects[i][Y_COORD] * 16 + 48 - graphicsInfo->height / 2,
            2);

        StartSpriteAnim(&gSprites[spriteId], gShopDataPtr->viewportObjects[i][ANIM_NUM]);
    }
}

static void BuyMenuCopyMenuBgToBg1TilemapBuffer(void)
{
    s16 i;
    u16 *dest = gShopDataPtr->tilemapBuffers[1];
    const u16 *src = gShopDataPtr->tilemapBuffers[0];

    for (i = 0; i < 1024; i++)
    {
        if (src[i] != 0)
        {
            dest[i] = src[i] + 0xB3DC;
        }
    }
}

static void Task_BuyMenu(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    if (!gPaletteFade.active)
    {
        s32 itemId = ListMenu_ProcessInput(tListTaskId);
        ListMenuGetScrollAndRow(tListTaskId, &gShopDataPtr->scrollOffset, &gShopDataPtr->selectedRow);

        switch (itemId)
        {
        case LIST_NOTHING_CHOSEN:
            break;
        case LIST_CANCEL:
            PlaySE(SE_SELECT);
            ExitBuyMenu(taskId);
            break;
        default:
            PlaySE(SE_SELECT);
            tItemId = itemId;
            ClearWindowTilemap(5);
            BuyMenuRemoveScrollIndicatorArrows();
            BuyMenuPrintCursor(tListTaskId, 2);
            sub_809B10C(1);

            if (gMartInfo.martType < MART_TYPE_DECOR)
            {
                gShopDataPtr->totalCost = (ItemId_GetPrice(itemId) >> GetPriceReduction(POKENEWS_SLATEPORT));
            }
            else
            {
                gShopDataPtr->totalCost = gDecorations[itemId].price;
            }

            if (!IsEnoughMoney(&gSaveBlock1Ptr->money, gShopDataPtr->totalCost))
            {
                BuyMenuDisplayMessage(taskId, gText_YouDontHaveMoney, BuyMenuReturnToItemList);
            }
            else
            {
                if (gMartInfo.martType < MART_TYPE_DECOR)
                {
                    CopyItemName(itemId, gStringVar1);
                    if (ItemId_GetPocket(itemId) == POCKET_TM_HM)
                    {
                        StringCopy(gStringVar2, gMoveNames[ItemIdToBattleMoveId(itemId)]);
                        BuyMenuDisplayMessage(taskId, gText_Var1CertainlyHowMany2, Task_BuyHowManyDialogueInit);
                    }
                    else
                    {
                        BuyMenuDisplayMessage(taskId, gText_Var1CertainlyHowMany, Task_BuyHowManyDialogueInit);
                    }
                }
                else
                {
                    StringCopy(gStringVar1, gDecorations[itemId].name);
                    ConvertIntToDecimalStringN(gStringVar2, gShopDataPtr->totalCost, STR_CONV_MODE_LEFT_ALIGN, 6);

                    if (gMartInfo.martType == MART_TYPE_DECOR)
                        StringExpandPlaceholders(gStringVar4, gText_Var1IsItThatllBeVar2);
                    else
                        StringExpandPlaceholders(gStringVar4, gText_YouWantedVar1ThatllBeVar2);
                    BuyMenuDisplayMessage(taskId, gStringVar4, BuyMenuConfirmPurchase);
                }
            }
            break;
        }
    }
}

static void Task_BuyHowManyDialogueInit(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    u16 quantityInBag = CountTotalItemQuantityInBag(tItemId);
    u16 maxQuantity;

    BuyMenuQuantityBoxThinBorder(1, 0);
    ConvertIntToDecimalStringN(gStringVar1, quantityInBag, STR_CONV_MODE_RIGHT_ALIGN, 3);
    StringExpandPlaceholders(gStringVar4, gText_InBagVar1);
    BuyMenuPrint(1, 1, gStringVar4, 0, 2, 0, 0, 0, 1);
    tItemCount = 1;
    BuyMenuQuantityBoxNormalBorder(3, 0);
    BuyMenuPrintItemQuantityAndPrice(taskId);
    schedule_bg_copy_tilemap_to_vram(0);

    maxQuantity = GetMoney(&gSaveBlock1Ptr->money) / gShopDataPtr->totalCost;

    if (maxQuantity > MAX_BAG_ITEM_CAPACITY)
    {
        gShopDataPtr->maxQuantity = MAX_BAG_ITEM_CAPACITY;
    }
    else
    {
        gShopDataPtr->maxQuantity = maxQuantity;
    }

    if (maxQuantity != 1)
        BuyQuantityAddScrollIndicatorArrows();
    
    gTasks[taskId].func = Task_BuyHowManyDialogueHandleInput;
}

static void Task_BuyHowManyDialogueHandleInput(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    if (AdjustQuantityAccordingToDPadInput(&tItemCount, gShopDataPtr->maxQuantity) == TRUE)
    {
        gShopDataPtr->totalCost = (ItemId_GetPrice(tItemId) >> GetPriceReduction(POKENEWS_SLATEPORT)) * tItemCount;
        BuyMenuPrintItemQuantityAndPrice(taskId);
    }
    else
    {
        if (gMain.newKeys & A_BUTTON)
        {
            PlaySE(SE_SELECT);
            BuyMenuRemoveScrollIndicatorArrows();
            ClearStdWindowAndFrameToTransparent(3, 0);
            ClearStdWindowAndFrameToTransparent(1, 0);
            ClearWindowTilemap(3);
            ClearWindowTilemap(1);
            PutWindowTilemap(4);
            CopyItemName(tItemId, gStringVar1);
            ConvertIntToDecimalStringN(gStringVar2, tItemCount, STR_CONV_MODE_LEFT_ALIGN, 2);
            ConvertIntToDecimalStringN(gStringVar3, gShopDataPtr->totalCost, STR_CONV_MODE_LEFT_ALIGN, 8);
            BuyMenuDisplayMessage(taskId, gText_Var1AndYouWantedVar2, BuyMenuConfirmPurchase);
        }
        else if (gMain.newKeys & B_BUTTON)
        {
            PlaySE(SE_SELECT);
            BuyMenuRemoveScrollIndicatorArrows();
            ClearStdWindowAndFrameToTransparent(3, 0);
            ClearStdWindowAndFrameToTransparent(1, 0);
            ClearWindowTilemap(3);
            ClearWindowTilemap(1);
            BuyMenuReturnToItemList(taskId);
        }
    }
}

static void BuyMenuConfirmPurchase(u8 taskId)
{
    CreateYesNoMenuWithCallbacks(taskId, &sShopBuyMenuYesNoWindowTemplates, 1, 0, 2, 1, 13, &sShopPurchaseYesNoFuncs);
}

static void BuyMenuTryMakePurchase(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    PutWindowTilemap(4);

    if (gMartInfo.martType < MART_TYPE_DECOR)
    {
        if (AddBagItem(tItemId, tItemCount) == TRUE)
        {
            BuyMenuDisplayMessage(taskId, gText_HereYouGoThankYou, BuyMenuSubtractMoney);
            RecordItemPurchase(taskId);
        }
        else
        {
            BuyMenuDisplayMessage(taskId, gText_NoMoreRoomForThis, BuyMenuReturnToItemList);
        }
    }
    else
    {
        if (DecorationAdd(tItemId))
        {
            if (gMartInfo.martType == MART_TYPE_DECOR)
            {
                BuyMenuDisplayMessage(taskId, gText_ThankYouIllSendItHome, BuyMenuSubtractMoney);
            }
            else
            {
                BuyMenuDisplayMessage(taskId, gText_ThanksIllSendItHome, BuyMenuSubtractMoney);
            }
        }
        else
        {
            BuyMenuDisplayMessage(taskId, gText_SpaceForVar1Full, BuyMenuReturnToItemList);
        }
    }
}

static void BuyMenuSubtractMoney(u8 taskId)
{
    IncrementGameStat(GAME_STAT_SHOPPED);
    RemoveMoney(&gSaveBlock1Ptr->money, gShopDataPtr->totalCost);
    PlaySE(SE_RG_SHOP);
    PrintMoneyAmountInMoneyBox(0, GetMoney(&gSaveBlock1Ptr->money), 0);

    if (gMartInfo.martType < MART_TYPE_DECOR)
    {
        gTasks[taskId].func = Task_ReturnToItemListAfterItemPurchase;
    }
    else
    {
        gTasks[taskId].func = Task_ReturnToItemListAfterDecorationPurchase;
    }
}

static void Task_ReturnToItemListAfterItemPurchase(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    if (gMain.newKeys & (A_BUTTON | B_BUTTON))
    {
        PlaySE(SE_SELECT);
        if (tItemId == ITEM_POKE_BALL && tItemCount > 9 && AddBagItem(ITEM_PREMIER_BALL, 1) == TRUE)
        {
            BuyMenuDisplayMessage(taskId, gText_ThrowInPremierBall, BuyMenuReturnToItemList);
        }
        else
        {
            BuyMenuReturnToItemList(taskId);
        }
    }
}

static void Task_ReturnToItemListAfterDecorationPurchase(u8 taskId)
{
    if (gMain.newKeys & (A_BUTTON | B_BUTTON))
    {
        PlaySE(SE_SELECT);
        BuyMenuReturnToItemList(taskId);
    }
}

static void BuyMenuReturnToItemList(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    ClearDialogWindowAndFrameToTransparent(2, 0);
    BuyMenuPrintCursor(tListTaskId, 1);
    sub_809B10C(0);
    PutWindowTilemap(4);
    PutWindowTilemap(5);
    if (gMartInfo.martType == MART_TYPE_TMHM)
        PutWindowTilemap(6);

    schedule_bg_copy_tilemap_to_vram(0);
    BuyMenuAddScrollIndicatorArrows();
    gTasks[taskId].func = Task_BuyMenu;
}

static void BuyMenuPrintItemQuantityAndPrice(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    FillWindowPixelBuffer(3, PIXEL_FILL(1));
    PrintMoneyAmount(3, 54, 10, gShopDataPtr->totalCost, TEXT_SPEED_FF);
    ConvertIntToDecimalStringN(gStringVar1, tItemCount, STR_CONV_MODE_LEADING_ZEROS, 2);
    StringExpandPlaceholders(gStringVar4, gText_xVar1);
    BuyMenuPrint(3, 0, gStringVar4, 2, 10, 0, 0, 0, 1);
}

static void ExitBuyMenu(u8 taskId)
{
    gFieldCallback = MapPostLoadHook_ReturnToShopMenu;
    BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB_BLACK);
    gTasks[taskId].func = Task_ExitBuyMenu;
}

static void Task_ExitBuyMenu(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        RemoveMoneyLabelObject();
        BuyMenuFreeMemory();
        SetMainCallback2(CB2_ReturnToField);
        DestroyTask(taskId);
    }
}

static void ClearItemPurchases(void)
{
    gMartPurchaseHistoryId = 0;
    memset(gMartPurchaseHistory, 0, sizeof(gMartPurchaseHistory));
}

static void RecordItemPurchase(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    u16 i;

    for (i = 0; i < 3; i++)
    {
        if (gMartPurchaseHistory[i].itemId == tItemId && gMartPurchaseHistory[i].quantity != 0)
        {
            if (gMartPurchaseHistory[i].quantity + tItemCount > 255)
            {
                gMartPurchaseHistory[i].quantity = 255;
            }
            else
            {
                gMartPurchaseHistory[i].quantity += tItemCount;
            }
            return;
        }
    }

    if (gMartPurchaseHistoryId < 3)
    {
        gMartPurchaseHistory[gMartPurchaseHistoryId].itemId = tItemId;
        gMartPurchaseHistory[gMartPurchaseHistoryId].quantity = tItemCount;
        gMartPurchaseHistoryId++;
    }
}

#undef tItemCount
#undef tItemId
#undef tListTaskId

void CreatePokemartMenu(const u16 *itemsForSale)
{
    SetShopItemsForSale(itemsForSale);
    CreateShopMenu(MART_TYPE_NORMAL);
    ClearItemPurchases();
    SetShopMenuCallback(EnableBothScriptContexts);
}

void CreateDecorationShop1Menu(const u16 *itemsForSale)
{
    SetShopItemsForSale(itemsForSale);
    CreateShopMenu(MART_TYPE_DECOR);
    SetShopMenuCallback(EnableBothScriptContexts);
}

void CreateDecorationShop2Menu(const u16 *itemsForSale)
{
    SetShopItemsForSale(itemsForSale);
    CreateShopMenu(MART_TYPE_DECOR2);
    SetShopMenuCallback(EnableBothScriptContexts);
}
