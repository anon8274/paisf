plot_format = "png"
plot_dpi = 300

# A palette of 16 colors (8 pairs) that are slightly less saturated (more pastel)
# than the previous list, maintaining the dark/light contrast.
colors = [
    # 1. Blue Pair
    "#3A4D99",  # Dark Muted Indigo
    "#82A9FF",  # Light Soft Blue

    # 2. Gold/Yellow Pair
    "#99804D",  # Dark Muted Mustard Gold
    "#FFEB85",  # Light Pale Yellow

    # 3. Green Pair
    "#407540",  # Dark Muted Pine Green
    "#99CC99",  # Light Sage Green

    # 4. Red Pair
    "#B34D4D",  # Dark Muted Rose Red
    "#FF9999",  # Light Dusty Rose

    # 5. Purple Pair
    "#8C4D8C",  # Dark Muted Amethyst
    "#CC99CC",  # Light Lilac

    # 6. Orange Pair
    "#99664D",  # Dark Muted Terra Cotta
    "#FFC299",  # Light Peach

    # 7. Cyan/Teal Pair
    "#4D7A7A",  # Dark Muted Slate Teal
    "#A3E0E0",  # Light Powder Blue/Cyan

    # 8. Brown/Tan Pair
    "#8A6E59",  # Dark Muted Taupe
    "#E0C2A3",  # Light Warm Beige
]

# Colors for reticle layers
reticle_color_by_layer = {str(i) : colors[i] for i in range(len(colors))}

# General list of markers
markers = [
    "o",   # Circle
    "s",   # Square
    "^",   # Triangle Up
    "D",   # Diamond
    "v",   # Triangle Down
    "P",   # Filled Plus (Thick Plus)
    "*",   # Star
    "X",   # Filled X (Thick X)
    "<",   # Triangle Left
    ">",   # Triangle Right
    "h",   # Hexagon 1
    "d",   # Thin Diamond
    "p",   # Pentagon
    "+",   # Plus
    "x",   # X
    "|",   # Vertical Line
]
