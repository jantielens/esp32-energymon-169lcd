# Power Screen Icons

Custom PNG icons for the energy monitoring display.

## Specifications

- **Size:** 48×48 pixels
- **Format:** PNG with alpha transparency
- **Color:** White/light gray (#FFFFFF or #CCCCCC)
- **Background:** Transparent
- **Style:** Simple, clear, minimalist line art

## Required Icons

### 1. solar.png - Solar Power Icon
- **Design:** Sun symbol
- **Suggested elements:** Circle with rays/spokes radiating outward
- **Example:** ☀ (but as a clean, simple line drawing)

### 2. home.png - Home Power Icon
- **Design:** House symbol
- **Suggested elements:** Simple house outline with roof and base
- **Example:** 🏠 (but as a clean, simple line drawing)

### 3. grid.png - Grid Power Icon
- **Design:** Power/electricity symbol
- **Suggested options:**
  - Lightning bolt (⚡)
  - Transmission tower
  - Electrical plug
- **Recommended:** Lightning bolt for simplicity

## Design Guidelines

1. **Keep it simple:** Icons will be displayed at 48×48px on a small LCD
2. **Use solid shapes:** Avoid thin lines that may not render clearly
3. **Center the design:** Leave some padding around edges
4. **Test visibility:** Icons should be recognizable even when recolored

## Color Recoloring

Icons will be dynamically recolored based on power levels:
- **Green:** Low/optimal power usage or solar generation
- **White:** Normal power usage
- **Orange:** Elevated power usage
- **Red:** High power usage

The icon design should work well in all these colors.

## Tools

Recommended tools for creating icons:
- **Inkscape:** Free vector graphics editor
- **GIMP:** Free raster graphics editor
- **Figma:** Web-based design tool
- **Icon generators:** Online tools like IconKitchen, Flaticon

## Conversion

Icons will be converted to LVGL C arrays using `tools/convert-icons.sh` which calls `lv_img_conv`:

```bash
./tools/convert-icons.sh
```

Output: `src/app/icons.h`

## Usage Example

```c
lv_obj_t* icon = lv_img_create(parent);
lv_img_set_src(icon, &icon_solar);
lv_obj_set_style_img_recolor(icon, COLOR_GREEN, LV_PART_MAIN);
lv_obj_set_style_img_recolor_opa(icon, LV_OPA_COVER, LV_PART_MAIN);
```
