#ifndef GUARD_ITEM_MENU_ICONS
#define GUARD_ITEM_MENU_ICONS

extern const struct CompressedSpriteSheet gBagMaleSpriteSheet;
extern const struct CompressedSpriteSheet gBagFemaleSpriteSheet;
extern const struct CompressedSpritePalette gBagMalePaletteTable;
extern const struct CompressedSpritePalette gBagFemalePaletteTable;
extern const struct CompressedSpriteSheet gBerryCheckCircleSpriteSheet;
extern const struct CompressedSpritePalette gBerryCheckCirclePaletteTable;

void RemoveBagSprite(u8 id);
void AddBagVisualSprite(u8 bagPocketId);
void SetBagVisualPocketId(u8 bagPocketId);
void ShakeBagSprite(void);
void AddSwitchPocketRotatingBallSprite(s16 rotationDirection);
void AddBagItemIconSprite(u16 itemId, u8 id);
void RemoveBagItemIconSprite(u8 id);
void ItemMenuIcons_CreateInsertIndicatorBarHidden(void);
void ItemMenuIcons_ToggleInsertIndicatorBarVisibility(u8 arg0);
void ItemMenuIcons_MoveInsertIndicatorBar(s16 x, s16 y);
u8 CreateBerryTagSprite(u8 id, s16 x, s16 y);
void FreeBerryTagSpritePalette(void);
u8 CreateSpinningBerrySprite(u8 berryId, u8 x, u8 y, bool8 startAffine);
u8 CreateBerryFlavorCircleSprite(s16 x);
void ResetItemMenuIconState(void);
void HideBagItemIconSprite(u8 id);

#define TAG_BAG_GFX 100
#define TAG_ROTATING_BALL_GFX 101
#define TAG_BERRY_CHECK_CIRCLE_GFX 10000
#define TAG_BERRY_PIC_TILE 0xFFFF
#define TAG_BERRY_PIC_PAL 0x7544

#endif // GUARD_ITEM_MENU_ICONS
