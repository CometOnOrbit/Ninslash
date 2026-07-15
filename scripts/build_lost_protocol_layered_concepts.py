#!/usr/bin/env python3
"""Build layered SVG redraw templates for the seven Lost Protocol machines."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "concepts" / "layered"

STYLE = """
  <defs><style>
    .armor{fill:#d8dbdd;stroke:#202328;stroke-width:10;stroke-linejoin:round;stroke-linecap:round}
    .armor2{fill:#aeb3b7;stroke:#202328;stroke-width:10;stroke-linejoin:round;stroke-linecap:round}
    .dark{fill:#4b5055;stroke:#202328;stroke-width:10;stroke-linejoin:round;stroke-linecap:round}
    .joint{fill:#71777c;stroke:#202328;stroke-width:9}.void{fill:#25282c;stroke:#202328;stroke-width:9}
    .blue{fill:#60bce8;stroke:#202328;stroke-width:9}.green{fill:#64ca8b;stroke:#202328;stroke-width:9}
    .orange{fill:#ef9941;stroke:#202328;stroke-width:9}.red{fill:#e6544e;stroke:#202328;stroke-width:9}
    .shine{fill:none;stroke:#fff;stroke-width:5;stroke-linecap:round;opacity:.9}
    .pivot{fill:#fff;stroke:#db3e3e;stroke-width:4}.pivotline{stroke:#db3e3e;stroke-width:3}
    .guide{fill:none;stroke:#7f96a5;stroke-width:3;stroke-dasharray:10 8}
    .label{font:22px 'Segoe UI',sans-serif;fill:#34434d}.small{font:17px 'Microsoft YaHei','Segoe UI',sans-serif;fill:#53626c}
  </style></defs>
"""


def layer(label: str, content: str, layer_id: str) -> str:
    return f'<g inkscape:groupmode="layer" inkscape:label="{label}" id="{layer_id}">{content}</g>'


def pivot(x: int, y: int, name: str) -> str:
    return (
        f'<circle class="pivot" cx="{x}" cy="{y}" r="9"/>'
        f'<path class="pivotline" d="M{x-15} {y}H{x+15}M{x} {y-15}V{x} {y+15}"/>'
        f'<text class="small" x="{x+14}" y="{y-13}">{name}</text>'
    )


def document(title: str, subtitle: str, layers: list[str], pivots: str, notes: list[str]) -> str:
    note_rows = "".join(f'<text class="small" x="720" y="{150+i*31}">• {text}</text>' for i, text in enumerate(notes))
    return f'''<svg xmlns="http://www.w3.org/2000/svg" xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape" width="1024" height="768" viewBox="0 0 1024 768">
{STYLE}
{layer("00 GUIDE — hide before export", f'<rect width="1024" height="768" fill="#f5f2ea"/><path class="guide" d="M40 650H680 M360 95V680"/><text class="label" x="38" y="48">{title}</text><text class="small" x="38" y="78">{subtitle}</text><text class="small" x="720" y="112">重绘要求 / redraw notes</text>{note_rows}', "guide")}
{''.join(layers)}
{layer("99 PIVOTS — keep coordinates", pivots, "pivots")}
</svg>'''


def bulwark() -> str:
    layers = [
        layer("01 rear upper leg [parent=body]", '<path class="dark" d="M244 455L205 526L235 543L286 470Z"/>', "rear_upper"),
        layer("02 rear lower leg [parent=rear_upper]", '<path class="armor2" d="M215 526L174 588L207 605L246 544Z"/>', "rear_lower"),
        layer("03 rear foot [parent=rear_lower]", '<path class="dark" d="M173 584L126 629L139 649H226L219 616Z"/>', "rear_foot"),
        layer("04 body shell [parent=root]", '<path class="armor" d="M132 302L190 245H423L487 303L470 431L401 471H197L123 416Z"/><path class="shine" d="M181 271H407"/>', "body"),
        layer("05 body eye/core [parent=body]", '<path class="void" d="M196 306H378L415 342L385 390H207L174 347Z"/><rect class="blue" x="233" y="329" width="119" height="39" rx="18"/>', "core"),
        layer("06 front upper leg [parent=body]", '<path class="dark" d="M394 453L427 525L396 544L350 470Z"/>', "front_upper"),
        layer("07 front lower leg [parent=front_upper]", '<path class="armor2" d="M426 524L466 588L434 605L393 543Z"/>', "front_lower"),
        layer("08 front foot [parent=front_lower]", '<path class="dark" d="M465 584L516 628L503 649H414L423 615Z"/>', "front_foot"),
        layer("09 shield arm [parent=body]", '<path class="dark" d="M430 311L516 272L556 311L488 375Z"/><circle class="joint" cx="458" cy="333" r="27"/><circle class="joint" cx="528" cy="302" r="25"/>', "shield_arm"),
        layer("10 upper shield [parent=shield_arm]", '<path class="armor" d="M520 137L624 161L672 232L651 339L586 381L502 348L482 236Z"/><path class="blue" d="M545 171L607 185L640 240L626 307L584 341L535 318L519 239Z"/><path class="shine" d="M544 172L606 186"/>', "shield_upper"),
        layer("11 lower shield [parent=shield_arm]", '<path class="armor" d="M540 333L651 360L690 426L659 503L566 509L510 457Z"/><path class="blue" d="M568 369L631 386L656 428L638 471L580 474L544 447Z"/><path class="shine" d="M569 369L630 386"/>', "shield_lower"),
    ]
    pivots = "".join((pivot(320, 397, "root/body"), pivot(244, 455, "rear hip"), pivot(215, 526, "rear knee"), pivot(394, 453, "front hip"), pivot(426, 524, "front knee"), pivot(458, 333, "shield base"), pivot(528, 302, "shield hinge")))
    return document("BULWARK / 壁垒机 — layered redraw template", "画布为结构参考；隐藏 GUIDE 与 PIVOTS 后分别导出命名图层", layers, pivots, ["盾墙占总高度约 55%", "机身低矮，不添加肩膀或手臂", "盾片边缘必须留完整遮挡区", "左右腿可共用一套贴图"])


def assembler() -> str:
    layers = [
        layer("01 rear reservoir [parent=body]", '<path class="armor" d="M149 132Q149 95 185 95H251Q286 95 286 132L273 311H161Z"/><rect class="green" x="181" y="133" width="73" height="124" rx="25"/><path class="shine" d="M174 116H260"/>', "reservoir"),
        layer("02 rear thruster [parent=body]", '<path class="dark" d="M224 490H281L267 570L251 595L234 570Z"/><path class="green" d="M239 518H266L253 570Z"/>', "thruster_rear"),
        layer("03 body shell [parent=root]", '<ellipse class="armor" cx="344" cy="386" rx="180" ry="123"/><path class="dark" d="M180 389Q344 305 507 389L479 446Q344 510 208 446Z"/><path class="shine" d="M220 319Q344 249 463 319"/>', "body"),
        layer("04 body core [parent=body]", '<ellipse class="green" cx="345" cy="383" rx="73" ry="45"/>', "core"),
        layer("05 top arm upper [parent=body]", '<path class="armor" d="M455 323L548 241L578 267L488 357Z"/><circle class="joint" cx="474" cy="337" r="26"/>', "arm_top_upper"),
        layer("06 top arm lower [parent=top_upper]", '<path class="dark" d="M554 235L628 178L653 208L583 273Z"/><circle class="joint" cx="574" cy="255" r="23"/>', "arm_top_lower"),
        layer("07 top tool [parent=top_lower]", '<path class="green" d="M635 165L686 144L714 164L698 195L657 207L629 188Z"/>', "tool_top"),
        layer("08 middle arm upper [parent=body]", '<path class="armor" d="M478 388L593 369L601 409L487 432Z"/><circle class="joint" cx="481" cy="410" r="25"/>', "arm_middle_upper"),
        layer("09 middle arm lower [parent=middle_upper]", '<path class="dark" d="M588 366L667 361L675 404L598 411Z"/><circle class="joint" cx="600" cy="390" r="22"/>', "arm_middle_lower"),
        layer("10 middle tool [parent=middle_lower]", '<path class="green" d="M669 347L726 344L746 374L725 405L675 407L656 378Z"/>', "tool_middle"),
        layer("11 lower arm upper [parent=body]", '<path class="armor" d="M445 462L529 528L500 563L414 493Z"/><circle class="joint" cx="429" cy="477" r="25"/>', "arm_lower_upper"),
        layer("12 lower arm lower [parent=lower_upper]", '<path class="dark" d="M522 523L584 574L555 611L493 558Z"/><circle class="joint" cx="504" cy="548" r="22"/>', "arm_lower_lower"),
        layer("13 lower tool [parent=lower_lower]", '<path class="green" d="M579 560L632 587L632 622L602 637L552 602Z"/>', "tool_lower"),
        layer("14 front thruster [parent=body]", '<path class="dark" d="M354 494H411L405 570L389 595L369 570Z"/><path class="green" d="M370 519H397L388 570Z"/>', "thruster_front"),
    ]
    pivots = "".join((pivot(344, 386, "root/body"), pivot(474, 337, "arm top"), pivot(574, 255, "top elbow"), pivot(481, 410, "arm middle"), pivot(600, 390, "middle elbow"), pivot(429, 477, "arm lower"), pivot(504, 548, "lower elbow")))
    return document("ASSEMBLER / 装配机 — layered redraw template", "三条机械臂必须保持不同长度与角度，避免复制粘贴感", layers, pivots, ["背部维修罐独立图层", "每条机械臂拆上臂/下臂/工具", "工具端可以重绘成焊枪、夹爪、维修球口", "推进器不可合并到机身"])


def saboteur() -> str:
    legs = []
    leg_data = (("rear_outer", 142, 447, 74, 558), ("rear_inner", 260, 455, 220, 574), ("front_inner", 435, 455, 475, 574), ("front_outer", 552, 447, 622, 558))
    for index, (name, hx, hy, fx, fy) in enumerate(leg_data, 1):
        midx, midy = (hx + fx) // 2, (hy + fy) // 2
        legs += [layer(f"{index:02d} {name} upper [parent=body]", f'<path class="dark" d="M{hx-14} {hy}L{midx-19} {midy}L{midx+11} {midy+17}L{hx+20} {hy+12}Z"/>', f"{name}_upper"), layer(f"{index:02d}b {name} lower+foot [parent={name}_upper]", f'<path class="armor2" d="M{midx-18} {midy}L{fx-14} {fy}L{fx+17} {fy+12}L{midx+13} {midy+17}Z"/><path class="dark" d="M{fx-15} {fy}L{fx-44} {fy+43}L{fx-31} {fy+57}H{fx+35}L{fx+25} {fy+34}Z"/>', f"{name}_lower")]
    layers = legs[:4] + [
        layer("05 low body shell [parent=root]", '<path class="dark" d="M104 346L171 273H496L610 342L567 431H151Z"/><path class="armor" d="M157 329L210 294H469L552 340L523 393H185Z"/><rect class="orange" x="239" y="350" width="211" height="39" rx="18"/>', "body"),
        layer("06 EMP coil [parent=body]", '<circle class="void" cx="355" cy="245" r="106"/><circle class="orange" cx="355" cy="245" r="70"/><circle class="void" cx="355" cy="245" r="31"/><path class="shine" d="M303 201A69 69 0 0 1 404 196"/>', "emp_coil"),
        layer("07 left antenna [parent=body]", '<path class="dark" d="M303 159L240 87L258 71L324 140Z"/><circle class="orange" cx="249" cy="79" r="15"/>', "antenna_left"),
        layer("08 right antenna [parent=body]", '<path class="dark" d="M404 158L472 87L490 104L421 178Z"/><circle class="orange" cx="481" cy="95" r="15"/>', "antenna_right"),
    ] + legs[4:]
    pivots = pivot(355, 374, "root/body") + pivot(355, 245, "EMP coil") + "".join(pivot(hx, hy, name) + pivot((hx+fx)//2, (hy+fy)//2, "knee") for name, hx, hy, fx, fy in leg_data)
    return document("SABOTEUR / 破坏机 — layered redraw template", "四足可以复用图，但每条腿在 Spine 中必须有独立 bone", layers, pivots, ["EMP 线圈必须是最大识别点", "低伏机身，不增加头部", "天线独立，蓄力时向后压", "足部需要完整接地轮廓"])


def railgunner() -> str:
    layers = [
        layer("01 rear upper leg [parent=body]", '<path class="armor" d="M270 412L226 530L263 546L316 426Z"/>', "rear_upper"),
        layer("02 rear lower leg [parent=rear_upper]", '<path class="dark" d="M227 526L178 613L211 631L269 544Z"/><path class="armor2" d="M177 608L139 652L153 674H239L230 645Z"/>', "rear_lower"),
        layer("03 main rail body [parent=root]", '<path class="dark" d="M111 253L183 194H721L847 238L899 286L845 328H183L108 295Z"/><path class="armor" d="M164 220H705L805 249L841 281L807 305H166L130 282Z"/><path class="shine" d="M198 230H691"/>', "body"),
        layer("04 charge telegraph strip [parent=body]", '<rect class="red" x="343" y="259" width="399" height="29" rx="14"/>', "telegraph_strip"),
        layer("05 rear eye [parent=body]", '<circle class="void" cx="166" cy="270" r="68"/><circle class="red" cx="166" cy="270" r="33"/>', "eye"),
        layer("06 muzzle telegraph emitter [parent=body]", '<path class="red" d="M832 235L913 256L947 282L906 309L829 329L861 282Z"/>', "muzzle"),
        layer("07 front upper leg [parent=body]", '<path class="armor" d="M664 412L709 530L672 546L620 426Z"/>', "front_upper"),
        layer("08 front lower leg [parent=front_upper]", '<path class="dark" d="M708 526L758 613L725 631L667 544Z"/><path class="armor2" d="M759 608L798 652L784 674H698L707 645Z"/>', "front_lower"),
    ]
    pivots = "".join((pivot(470, 281, "root/body"), pivot(270, 412, "rear hip"), pivot(227, 526, "rear knee"), pivot(664, 412, "front hip"), pivot(708, 526, "front knee"), pivot(843, 282, "muzzle")))
    return document("RAILGUNNER / 轨道炮手 — layered redraw template", "炮身保持一体长轮廓；两腿只负责支撑", layers, pivots, ["炮管占总长约 70%", "眼/传感器位于炮尾，不做头部", "预告发光条独立图层", "腿必须能完成后坐下蹲"])


def siege() -> str:
    layers = []
    leg_data = (("rear", 184, 113), ("rear_mid", 315, 276), ("middle", 447, 447), ("front_mid", 579, 619), ("front", 693, 761))
    for index, (name, hx, footx) in enumerate(leg_data):
        upper_number = index * 2 + 1
        lower_number = upper_number + 1
        layers += [
            layer(f"{upper_number:02d} {name} upper leg [parent=chassis]", f'<path class="armor" d="M{hx-18} 463L{footx-15} 559L{footx+20} 576L{hx+22} 478Z"/>', f"{name}_upper"),
            layer(f"{lower_number:02d} {name} lower leg and foot [parent={name}_upper]", f'<path class="dark" d="M{footx-15} 558L{footx-50} 642L{footx-18} 660L{footx+25} 580Z"/><path class="armor2" d="M{footx-49} 637L{footx-82} 682L{footx-68} 704H{footx+19}L{footx+10} 674Z"/>', f"{name}_lower"),
        ]
    layers += [
        layer("11 chassis shell [parent=root]", '<path class="dark" d="M92 349L172 267H624L745 326L789 419L718 476H164L77 414Z"/><path class="armor" d="M139 344L196 294H597L693 340L720 405L677 438H187L112 397Z"/><rect class="orange" x="191" y="376" width="144" height="33" rx="16"/><rect class="red" x="442" y="376" width="190" height="33" rx="16"/>', "chassis"),
        layer("12 phase shutter left [parent=chassis]", '<path class="dark" d="M338 327L382 327L382 431L338 431Z"/><path class="armor2" d="M351 342H369V416H351Z"/>', "phase_shutter_left"),
        layer("13 phase shutter right [parent=chassis]", '<path class="dark" d="M510 327L554 327L554 431L510 431Z"/><path class="armor2" d="M523 342H541V416H523Z"/>', "phase_shutter_right"),
        layer("14 mine rack [parent=chassis]", '<path class="dark" d="M133 287L209 202L274 207L299 273L251 315Z"/><circle class="orange" cx="193" cy="258" r="24"/><circle class="orange" cx="247" cy="254" r="24"/>', "mine_rack"),
        layer("15 turret base [parent=chassis]", '<circle class="joint" cx="459" cy="282" r="48"/><path class="armor" d="M317 201L398 151L603 157L664 211L615 274H348Z"/><path class="shine" d="M357 187L581 180"/>', "turret"),
        layer("16 cannon [parent=turret]", '<path class="dark" d="M598 178L830 188L947 226L964 260L890 284L615 269Z"/><rect class="red" x="660" y="222" width="219" height="24" rx="12"/><path class="shine" d="M653 205L824 211"/>', "cannon"),
    ]
    pivots = pivot(432, 388, "root/chassis") + pivot(459, 282, "turret") + "".join(pivot(hx, 463, f"{name} hip") for name, hx, _ in (("rear",184,113),("rear_mid",315,276),("middle",447,447),("front_mid",579,619),("front",693,761)))
    return document("SIEGE ENGINE / 攻城引擎 — layered redraw template", "Boss 通过宽度、腿数与独立炮塔体现体量", layers, pivots, ["炮塔与炮管分层", "地雷架独立，可在阶段切换时外翻", "五条可见腿；另一侧可复用镜像", "装甲百叶需预留打开后的内部结构"])


def drone() -> str:
    layers = [
        layer("01 assault core shell [parent=root]", '<circle class="dark" cx="192" cy="355" r="92"/><circle class="armor" cx="192" cy="355" r="70"/><circle class="void" cx="192" cy="355" r="39"/>', "assault_core"),
        layer("02 assault central eye [parent=core]", '<circle class="red" cx="192" cy="355" r="20"/>', "assault_eye"),
        layer("03 assault weapon mount [parent=core]", '<path class="dark" d="M254 322L313 312L333 337L314 370L252 370Z"/><circle class="joint" cx="271" cy="346" r="17"/>', "assault_mount"),
        layer("04 assault pulse pod [parent=weapon_mount]", '<path class="armor" d="M316 308L412 318L446 347L413 378L316 384L292 346Z"/><rect class="red" x="342" y="334" width="71" height="25" rx="11"/>', "assault_pod"),
        layer("05 guardian core shell [reuse assault core art]", '<circle class="dark" cx="506" cy="355" r="92"/><circle class="armor" cx="506" cy="355" r="70"/><circle class="void" cx="506" cy="355" r="39"/>', "guardian_core"),
        layer("06 guardian central eye [color variant]", '<circle class="blue" cx="506" cy="355" r="20"/>', "guardian_eye"),
        layer("07 guardian upper wing [parent=core]", '<path class="armor" d="M481 282L524 210L579 195L605 226L571 294L522 312Z"/><path class="blue" d="M512 277L539 231L571 222L583 234L559 278L530 290Z"/>', "guardian_upper_wing"),
        layer("08 guardian lower wing [parent=core]", '<path class="armor" d="M483 428L526 500L581 514L606 483L571 415L522 397Z"/><path class="blue" d="M513 433L540 479L571 488L584 476L560 432L531 420Z"/>', "guardian_lower_wing"),
        layer("09 repair core shell [reuse assault core art]", '<circle class="dark" cx="805" cy="355" r="92"/><circle class="armor" cx="805" cy="355" r="70"/><circle class="void" cx="805" cy="355" r="39"/>', "repair_core"),
        layer("10 repair central eye [color variant]", '<circle class="green" cx="805" cy="355" r="20"/>', "repair_eye"),
        layer("11 repair arm upper [parent=core]", '<path class="armor" d="M864 322L918 285L941 311L891 354Z"/><circle class="joint" cx="875" cy="337" r="18"/>', "repair_upper"),
        layer("12 repair arm lower [parent=repair_upper]", '<path class="dark" d="M920 281L968 251L985 278L944 315Z"/><circle class="joint" cx="936" cy="298" r="16"/>', "repair_lower"),
        layer("13 repair claw [parent=repair_lower]", '<path class="green" d="M969 238L1004 220L1020 238L1004 257L985 260L1006 278L995 300L967 283L949 260Z"/>', "repair_claw"),
    ]
    pivots = "".join((pivot(192, 355, "assault core"), pivot(271, 346, "weapon"), pivot(506, 355, "guardian core"), pivot(506, 274, "upper wing"), pivot(506, 436, "lower wing"), pivot(805, 355, "repair core"), pivot(875, 337, "repair arm"), pivot(936, 298, "repair elbow")))
    return document("PVE DRONE / 玩家无人机 — layered redraw template", "造型保持不变；三种模块并排展示，共用同一圆形核心比例", layers, pivots, ["红：Assault 脉冲武器舱", "蓝：Guardian 上下盾翼", "绿：Repair 两段维修臂与夹爪", "三种核心壳可只重绘一次后复用"])


def overseer() -> str:
    layers = [
        layer("01 rear thruster cluster [parent=core]", '<path class="dark" d="M370 526L408 526L397 591L385 614L372 591Z"/><path class="blue" d="M379 548H397L388 590Z"/><path class="dark" d="M470 526L508 526L497 591L485 614L472 591Z"/><path class="blue" d="M479 548H497L488 590Z"/>', "thrusters"),
        layer("02 outer core shell [parent=root]", '<circle class="dark" cx="440" cy="359" r="192"/><circle class="armor" cx="440" cy="359" r="158"/><path class="armor2" d="M300 303Q440 210 580 303L558 422Q440 495 322 422Z"/><path class="shine" d="M334 259Q440 205 542 259"/>', "core_shell"),
        layer("03 central eye housing [parent=core]", '<circle class="void" cx="440" cy="359" r="92"/>', "iris_housing"),
        layer("04 central iris ring [parent=eye_housing]", '<circle class="red" cx="440" cy="359" r="58"/>', "iris_ring"),
        layer("05 central pupil [parent=iris_ring]", '<circle class="void" cx="440" cy="359" r="27"/>', "iris_pupil"),
        layer("06 top node arm [parent=core]", '<path class="armor" d="M421 185L421 104L459 104L459 185Z"/><circle class="joint" cx="440" cy="187" r="22"/>', "arm_top"),
        layer("07 top shield node [parent=arm_top]", '<circle class="dark" cx="440" cy="87" r="45"/><circle class="blue" cx="440" cy="87" r="22"/>', "node_top"),
        layer("08 left node arm [parent=core]", '<path class="armor" d="M300 405L225 452L204 419L281 373Z"/><circle class="joint" cx="291" cy="390" r="22"/>', "arm_left"),
        layer("09 left shield node [parent=arm_left]", '<circle class="dark" cx="195" cy="447" r="45"/><circle class="blue" cx="195" cy="447" r="22"/>', "node_left"),
        layer("10 right node arm [parent=core]", '<path class="armor" d="M580 405L655 452L676 419L599 373Z"/><circle class="joint" cx="589" cy="390" r="22"/>', "arm_right"),
        layer("11 right shield node [parent=arm_right]", '<circle class="dark" cx="685" cy="447" r="45"/><circle class="blue" cx="685" cy="447" r="22"/>', "node_right"),
        layer("12 phase shutter left [parent=core]", '<path class="dark" d="M309 286L250 219L275 196L340 253Z"/>', "shutter_left"),
        layer("13 phase shutter right [parent=core]", '<path class="dark" d="M571 286L630 219L605 196L540 253Z"/>', "shutter_right"),
    ]
    pivots = "".join((pivot(440, 359, "root/core"), pivot(440, 187, "top arm"), pivot(440, 87, "top node"), pivot(291, 390, "left arm"), pivot(195, 447, "left node"), pivot(589, 390, "right arm"), pivot(685, 447, "right node")))
    return document("OVERSEER CORE / 监管核心 — layered redraw template", "造型保持现有圆形核心、红色中央眼与三枚护盾节点", layers, pivots, ["中央眼必须与外壳分层", "三条节点臂与节点分别导出", "阶段百叶独立，便于 Boss 转阶段", "推进器可共用一张贴图"])


COMPONENT_DOCS = {
    "01_bulwark": """# Bulwark / 壁垒机部件清单\n\n- **body_shell**：主体外壳；父级 `root`。\n- **body_core**：蓝色识别核心；父级 `body_shell`。\n- **rear_upper_leg / rear_lower_leg / rear_foot**：后腿三段。\n- **front_upper_leg / front_lower_leg / front_foot**：前腿三段；允许复用后腿贴图。\n- **shield_arm**：盾墙连接臂；需完整画出被盾遮挡部分。\n- **shield_upper / shield_lower**：两块独立盾片；蓝色内层不得与外框合并。\n- **可复用**：关节圆盘、左右腿。\n- **组装方式**：所有导出的全画布 PNG 放在相同坐标 `(0,0)` 即可自动还原组装图。\n""",
    "02_assembler": """# Assembler / 装配机部件清单\n\n- **body_shell / body_core**：悬浮主体和绿色核心。\n- **rear_reservoir**：背部维修罐，必须独立。\n- **rear_thruster / front_thruster**：两枚推进器，可复用一张贴图。\n- **top_arm_upper / top_arm_lower / top_tool**：上方维修臂三段。\n- **middle_arm_upper / middle_arm_lower / middle_tool**：中部维修臂三段。\n- **lower_arm_upper / lower_arm_lower / lower_tool**：下方维修臂三段。\n- **可复用**：肘关节、推进器；三种工具端建议保持不同轮廓。\n- **组装方式**：全画布 PNG 均保持 1024×768，不要裁切位置。\n""",
    "03_saboteur": """# Saboteur / 破坏机部件清单\n\n- **body_shell**：低伏主机身。\n- **emp_coil**：顶部 EMP 线圈，需保留外环、发光环和中心三层。\n- **antenna_left / antenna_right**：两根独立天线。\n- **rear_outer / rear_inner / front_inner / front_outer leg**：四条腿，每条拆上段和下段/脚。\n- **可复用**：腿部贴图可以镜像复用，但 Spine 中仍为四套独立骨骼。\n- **组装方式**：线圈位于机身上方，不与主壳合并。\n""",
    "04_railgunner": """# Railgunner / 轨道炮手部件清单\n\n- **main_rail_body**：完整炮身主体。\n- **rear_eye**：炮尾传感器。\n- **muzzle_emitter**：炮口与预告线发射器。\n- **charge_telegraph_strip**：炮身红色蓄力条，必须独立，便于闪烁和充能动画。\n- **rear_upper_leg / rear_lower_leg**：后支撑腿。\n- **front_upper_leg / front_lower_leg**：前支撑腿。\n- **可复用**：两条腿的装甲片。\n- **组装方式**：炮身是 root；腿从炮身下方两个枢轴连接。\n""",
    "05_siege_engine": """# Siege Engine / 攻城引擎部件清单\n\n- **chassis_shell**：Boss 低矮底盘。\n- **mine_rack**：左后方地雷架。\n- **turret_base / cannon**：独立旋转炮塔和炮管。\n- **phase_shutter_left / phase_shutter_right**：两块独立阶段百叶，打开时显示内部结构。\n- **rear_upper / rear_lower**：最后方腿的上、下段。\n- **rear_mid_upper / rear_mid_lower**：后中腿的上、下段。\n- **middle_upper / middle_lower**：中央腿的上、下段。\n- **front_mid_upper / front_mid_lower**：前中腿的上、下段。\n- **front_upper / front_lower**：最前方腿的上、下段。\n- **far_side_legs**：另一侧腿可复用贴图并在 Spine 中降低层级。\n- **组装方式**：炮塔枢轴位于底盘上方中央，炮管枢轴位于炮塔前缘。\n""",
    "06_pve_drone": """# PvE Drone / 玩家无人机部件清单\n\n- **base_core_shell**：三模块共用的圆形核心壳；三张位置参考只需重绘一次。\n- **assault / guardian / repair central_eye**：同一眼部图形的红、蓝、绿状态，可用换色皮肤实现。\n- **assault_weapon_mount / assault_pulse_pod**：红色 Assault 模块。\n- **guardian_upper_wing / guardian_lower_wing**：蓝色 Guardian 盾翼。\n- **repair_arm_upper / repair_arm_lower / repair_claw**：绿色 Repair 模块。\n- **不可合并**：眼、武器舱、上下盾翼和两段维修臂必须各自独立。\n- **组装方式**：组件原图中展示三种配置；实际游戏按模块只加载对应附件。\n""",
    "07_overseer_core": """# Overseer Core / 监管核心部件清单\n\n- **outer_core_shell**：圆形主体外壳。\n- **central_eye_housing**：中央红眼的黑色外壳。\n- **central_iris_ring**：独立红色虹膜环，用于阶段脉冲和受击动画。\n- **central_pupil**：独立中心孔。\n- **top/left/right node_arm**：三条护盾节点臂。\n- **top/left/right shield_node**：三枚独立蓝色节点。\n- **phase_shutter_left / phase_shutter_right**：阶段切换百叶。\n- **rear_thruster_cluster**：底部推进器；左右喷口可复用。\n- **组装方式**：节点臂围绕核心 root 旋转，节点是各自手臂的子级。\n""",
}


COMPONENT_DOC_FOOTER = """
## 文件与放置方式

- `*_layered.svg`：可编辑的分层原图；打开后可看到完整组装位置和骨骼枢轴。
- `*_layered.png`：带说明与枢轴的组装预览，只用于对照。
- `*_assembled_transparent.png`：隐藏说明后的透明组装基准图。
- `*_components.svg` / `*_components.png`：把所有部件拆开陈列的组件原图。
- `parts/<单位>/cropped/`：裁切后的透明部件，适合单独查看和重绘。
- `parts/<单位>/full_canvas/`：每张均为 `1024×768`；按文件编号从小到大叠放，且每层位置都设为 `(0,0)`，即可逐像素还原透明组装基准图。
- 不要自动裁切 `full_canvas`、不要居中图层、不要改画布尺寸；这些透明边距就是组件位置数据。
"""


def readme() -> str:
    return """# Lost Protocol 分层重绘交付包

每个 SVG 都使用 Inkscape/Illustrator 可识别的命名图层。请在对应图层内重绘，不要合并图层，也不要修改 `PIVOTS` 图层中的十字坐标。

## 工作方式

1. 打开单个单位 SVG。
2. 在现有命名图层中替换草图形状；允许改变外轮廓，但保持父子关系。
3. 被关节遮住的位置需要继续画入关节下方 8–16px，防止旋转时露缝。
4. 保持统一侧视、朝右、透明背景；不画地面、阴影、文字或发光外溢。
5. `GUIDE` 和 `PIVOTS` 只用于制作，最终导出前隐藏。

## 不会放置组件时

1. 新建 `1024×768` 透明画布。
2. 从 `parts/<单位>/full_canvas/` 导入全部 PNG。
3. 每一层都放在左上角坐标 `(0,0)`，不要执行自动裁切、居中或对齐。
4. 按文件名前两位编号从小到大叠放；编号越大越靠前。
5. 结果应与同名 `*_assembled_transparent.png` 完全一致。

`cropped/` 是方便画师看清部件的裁切副本，不携带组装坐标；`full_canvas/` 才是带原始位置的组件文件。

## 最终回传

- 首选：保留图层的 SVG、PSD、KRA 或 AFDesign 源文件。
- 同时提供一张透明背景的完整组装 PNG，便于 128px 尺寸审核。
- 不必自行打 atlas；Codex 会按图层导出 PNG、设置骨骼、制作 Spine 3.6 JSON/atlas 和动画。

## 风格

参考 Ninslash 原版 Walker、Crawler、Star Droid：灰白圆壳体、低零件密度、粗黑描边、有限阴影。功能色只放在核心识别部件，不要增加通用科幻机甲刻线。
"""


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    files = {
        "01_bulwark_layered.svg": bulwark(),
        "02_assembler_layered.svg": assembler(),
        "03_saboteur_layered.svg": saboteur(),
        "04_railgunner_layered.svg": railgunner(),
        "05_siege_engine_layered.svg": siege(),
        "06_pve_drone_layered.svg": drone(),
        "07_overseer_core_layered.svg": overseer(),
        "README_CN.md": readme(),
    }
    files.update({f"{name}_COMPONENTS.md": content.rstrip() + "\n" + COMPONENT_DOC_FOOTER for name, content in COMPONENT_DOCS.items()})
    for name, content in files.items():
        (OUT / name).write_text(content, encoding="utf-8", newline="\n")
        print(OUT / name)


if __name__ == "__main__":
    main()
