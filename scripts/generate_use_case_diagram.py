#!/usr/bin/env python3
"""Generate UML-style use case diagram for 洗濯機君 (PNG + PDF)."""

from pathlib import Path

import matplotlib.pyplot as plt
from matplotlib.patches import Ellipse, FancyBboxPatch, Circle, Polygon
import matplotlib.font_manager as fm

_NOTO_REG = Path("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc")
_NOTO_BOLD = Path("/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc")
if _NOTO_REG.is_file():
    fm.fontManager.addfont(str(_NOTO_REG))
    FONT_REG = fm.FontProperties(fname=str(_NOTO_REG))
    FONT_BOLD = (
        fm.FontProperties(fname=str(_NOTO_BOLD))
        if _NOTO_BOLD.is_file()
        else FONT_REG
    )
    plt.rcParams["font.family"] = FONT_REG.get_name()
else:
    FONT_REG = FONT_BOLD = None
    plt.rcParams["font.family"] = "DejaVu Sans"
plt.rcParams["axes.unicode_minus"] = False


def _text_kwargs(bold: bool = False, extra: dict | None = None) -> dict:
    if FONT_REG:
        kw = {"fontproperties": FONT_BOLD if bold else FONT_REG}
    else:
        kw = {"fontweight": "bold"} if bold else {}
    if extra:
        kw.update(extra)
    return kw

OUT_DIR = Path(__file__).resolve().parents[1] / "docs" / "diagrams"
OUT_DIR.mkdir(parents=True, exist_ok=True)

# Layout constants (figure coordinates 0–100)
SYSTEM = {"x": 22, "y": 8, "w": 56, "h": 84}

USE_CASES = [
    (50, 76, "洗濯開始を検知"),
    (50, 66, "洗濯終了を検知"),
    (50, 56, "洗濯完了を通知"),
    (50, 46, "干し忘れをリマインド"),
    (50, 36, "干し作業を確認"),
    (50, 26, "夜間モードでスリープ"),
    (50, 16, "センサー接続を確認"),
    (38, 6, "時刻を同期"),
]

ACTORS = [
    # (name, x, y, side) side: 'left' | 'right'
    ("ユーザー", 8, 50, "left"),
    ("洗濯機", 8, 22, "left"),
    ("Discord", 92, 58, "right"),
    ("NTPサーバー", 92, 12, "right"),
]

# (actor_index, use_case_index or (cx,cy), linestyle)
LINKS = [
    (0, 2),  # user - notify complete
    (0, 3),  # user - snooze reminder
    (0, 4),  # user - hang laundry check
    (1, 0),  # washer - start
    (1, 1),  # washer - end
    (2, 2),  # discord - notify
    (2, 3),  # discord - remind
    (2, 6),  # discord - sensor error (via notify) — link to 通知系
    (3, 7),  # ntp - time sync (index 7 is 時刻を同期 at position 38,6)
]

# Fix links for 時刻を同期 - it's index 7 in USE_CASES
# Discord also for 初回 - we could add but keep 8 use cases

INCLUDE = [(0, 1, "«include»")]  # 終了検知 includes 開始の監視 chain - skip for clarity

EXTEND = [(3, 4, "«extend»")]  # リマインド extends after 完了通知 conceptually


def draw_actor(ax, x, y, name):
    """Stick figure + label."""
    head = Circle((x, y + 5), 2.2, fill=False, linewidth=1.4, edgecolor="#333333")
    ax.add_patch(head)
    ax.plot([x, x], [y + 2.8, y - 4], color="#333333", linewidth=1.4)
    ax.plot([x - 3.5, x + 3.5], [y - 1, y - 1], color="#333333", linewidth=1.4)
    ax.plot([x, x - 3], [y - 4, y - 9], color="#333333", linewidth=1.4)
    ax.plot([x, x + 3], [y - 4, y - 9], color="#333333", linewidth=1.4)
    ax.text(
        x,
        y - 13,
        name,
        ha="center",
        va="top",
        fontsize=11,
        **_text_kwargs(bold=True),
    )


def use_case_center(idx):
    cx, cy, _ = USE_CASES[idx]
    return cx, cy


def draw_use_case(ax, cx, cy, label):
    w, h = 22, 5.5
    ell = Ellipse(
        (cx, cy),
        w,
        h,
        facecolor="#E8F4FC",
        edgecolor="#1A5F8C",
        linewidth=1.3,
    )
    ax.add_patch(ell)
    ax.text(cx, cy, label, ha="center", va="center", fontsize=9.5, **_text_kwargs())


def uc_edge_point(cx, cy, toward_x, toward_y):
    """Point on ellipse boundary toward another point."""
    import math

    dx, dy = toward_x - cx, toward_y - cy
    if dx == 0 and dy == 0:
        return cx, cy
    angle = math.atan2(dy / 2.75, dx / 11.0)  # approximate ellipse
    return cx + 11.0 * math.cos(angle), cy + 2.75 * math.sin(angle)


def actor_anchor(x, y, side):
    if side == "left":
        return x + 4, y
    return x - 4, y


def main():
    fig, ax = plt.subplots(figsize=(14, 10), dpi=150)
    ax.set_xlim(0, 100)
    ax.set_ylim(0, 100)
    ax.set_aspect("equal")
    ax.axis("off")

    # Title
    ax.text(
        50,
        97,
        "洗濯機君 — ユースケース図",
        ha="center",
        va="top",
        fontsize=16,
        color="#1a1a1a",
        **_text_kwargs(bold=True),
    )
    ax.text(
        50,
        93.5,
        "Seeed XIAO ESP32C3 + MPU6050 / Discord Webhook",
        ha="center",
        va="top",
        fontsize=9,
        color="#555555",
    )

    # System boundary
    box = FancyBboxPatch(
        (SYSTEM["x"], SYSTEM["y"]),
        SYSTEM["w"],
        SYSTEM["h"],
        boxstyle="round,pad=0.02,rounding_size=1.5",
        linewidth=2,
        edgecolor="#2C5282",
        facecolor="#FAFBFC",
        linestyle="-",
    )
    ax.add_patch(box)
    ax.text(
        SYSTEM["x"] + SYSTEM["w"] / 2,
        SYSTEM["y"] + SYSTEM["h"] - 2.5,
        "洗濯機君システム",
        ha="center",
        va="top",
        fontsize=12,
        color="#2C5282",
        **_text_kwargs(bold=True),
    )

    for cx, cy, label in USE_CASES:
        draw_use_case(ax, cx, cy, label)

    actor_meta = []
    for name, x, y, side in ACTORS:
        draw_actor(ax, x, y, name)
        actor_meta.append((name, x, y, side))

    for ai, ui in LINKS:
        ax_name, ax_x, ax_y, side = actor_meta[ai]
        uc_x, uc_y = use_case_center(ui)
        ax_pt = actor_anchor(ax_x, ax_y, side)
        uc_pt = uc_edge_point(uc_x, uc_y, ax_pt[0], ax_pt[1])
        ax.plot(
            [ax_pt[0], uc_pt[0]],
            [ax_pt[1], uc_pt[1]],
            color="#666666",
            linewidth=1.0,
            linestyle="-",
        )

    # 洗濯完了を通知 —include→ 洗濯終了を検知
    ax.annotate(
        "",
        xy=use_case_center(1),
        xytext=use_case_center(2),
        arrowprops=dict(arrowstyle="-|>", color="#888888", lw=1.0, linestyle="dashed"),
    )
    ax.text(57, 60, "«include»", fontsize=7.5, color="#777777", ha="left")

    # 干し忘れをリマインド —extend→ 洗濯完了を通知
    ax.annotate(
        "",
        xy=use_case_center(2),
        xytext=use_case_center(3),
        arrowprops=dict(arrowstyle="-|>", color="#888888", lw=1.0, linestyle="dashed"),
    )
    ax.text(57, 50, "«extend»", fontsize=7.5, color="#777777", ha="left")

    # Legend note
    ax.text(
        50,
        2,
        "※ 各ユースケースは Deep Sleep 起床後の短い処理サイクルで実行されます",
        ha="center",
        va="bottom",
        fontsize=8,
        color="#666666",
        **_text_kwargs(),
    )

    png_path = OUT_DIR / "use-case-diagram.png"
    pdf_path = OUT_DIR / "use-case-diagram.pdf"
    fig.savefig(png_path, bbox_inches="tight", facecolor="white", pad_inches=0.3)
    fig.savefig(pdf_path, bbox_inches="tight", facecolor="white", pad_inches=0.3)
    plt.close(fig)
    print(f"Wrote {png_path}")
    print(f"Wrote {pdf_path}")


if __name__ == "__main__":
    main()
