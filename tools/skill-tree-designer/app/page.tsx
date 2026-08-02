"use client";

import { ChangeEvent, PointerEvent, useEffect, useMemo, useRef, useState } from "react";

type Branch = "root" | "axe" | "pickaxe" | "club" | "hybrid" | "mastery";
type SkillNode = {
  id: string;
  title: string;
  description: string;
  branch: Branch;
  cost: number;
  x: number;
  y: number;
};
type Edge = { from: string; to: string };
type TreeState = { nodes: SkillNode[]; edges: Edge[]; notes: string[] };

const branchMeta: Record<Branch, { label: string; icon: string }> = {
  root: { label: "Исток", icon: "✦" },
  axe: { label: "Топор", icon: "◒" },
  pickaxe: { label: "Кирка", icon: "◆" },
  club: { label: "Дубинка", icon: "●" },
  hybrid: { label: "Гибрид", icon: "◇" },
  mastery: { label: "Мастерство", icon: "✹" },
};

const initialTree: TreeState = {
  nodes: [
    { id: "spark", title: "Искра ремесла", description: "После первой пережитой ночи открывает выбор первого настоящего инструмента.", branch: "root", cost: 1, x: 900, y: 720 },
    { id: "axe", title: "Каменный топор", description: "Рубка деревьев становится отдельным быстрым действием.", branch: "axe", cost: 1, x: 460, y: 500 },
    { id: "pickaxe", title: "Каменная кирка", description: "Позволяет эффективно разбивать камень и кристаллы.", branch: "pickaxe", cost: 1, x: 900, y: 360 },
    { id: "club", title: "Тяжёлая дубинка", description: "Первое оружие: медленное, грубое и способное отбрасывать.", branch: "club", cost: 1, x: 1340, y: 500 },

    { id: "clean-cut", title: "Чистый срез", description: "Удар по подсвеченной зоне даёт цельное бревно вместо россыпи мелких ресурсов.", branch: "axe", cost: 2, x: 170, y: 300 },
    { id: "falling-lesson", title: "Куда падает лес", description: "Удержание удара задаёт направление падения; ствол повреждает врагов и постройки.", branch: "axe", cost: 3, x: 140, y: 610 },
    { id: "resin-hunter", title: "Охотник за смолой", description: "Редкие деревья видны издалека и дают смолу для огня, ловушек и взрывчатки.", branch: "axe", cost: 3, x: 350, y: 80 },
    { id: "living-wood", title: "Живая древесина", description: "Свежие брёвна можно сразу превратить во временные мосты и подпорки.", branch: "axe", cost: 4, x: 30, y: 30 },

    { id: "weak-spots", title: "Чтение трещин", description: "На камне появляются перемещающиеся слабые точки; точный удар ускоряет раскол.", branch: "pickaxe", cost: 2, x: 710, y: 70 },
    { id: "deep-vein", title: "Глубокая жила", description: "Разрушенный камень иногда открывает короткую кристальную жилу, которую надо успеть выбрать.", branch: "pickaxe", cost: 3, x: 900, y: 20 },
    { id: "echo-stone", title: "Каменное эхо", description: "Удар посылает импульс и на несколько секунд отмечает ресурсы внутри скалы.", branch: "pickaxe", cost: 3, x: 1080, y: 70 },
    { id: "fault-line", title: "Линия разлома", description: "Три точных удара соединяют трещины и раскалывают сразу группу соседних камней.", branch: "pickaxe", cost: 4, x: 840, y: 190 },

    { id: "wide-swing", title: "Широкий замах", description: "Заряженный удар дубинкой задевает несколько целей и расчищает пространство.", branch: "club", cost: 2, x: 1660, y: 300 },
    { id: "brace", title: "Упереться", description: "Удержание дубинки перед собой блокирует толчок, но расходует выносливость.", branch: "club", cost: 2, x: 1660, y: 610 },
    { id: "return-blow", title: "Ответный удар", description: "Идеальный блок мгновенно заряжает следующий замах и усиливает отбрасывание.", branch: "club", cost: 3, x: 1450, y: 90 },
    { id: "ground-pound", title: "Удар оземь", description: "Удар в прыжке создаёт круговую волну, но на секунду оставляет игрока беззащитным.", branch: "club", cost: 4, x: 1800, y: 60 },

    { id: "right-tool", title: "Верный инструмент", description: "Топор и кирка автоматически сменяются при наведении на подходящий ресурс.", branch: "hybrid", cost: 2, x: 610, y: 300 },
    { id: "breaker", title: "Рабочий замах", description: "Добывающий инструмент можно использовать в бою; точное попадание оглушает.", branch: "hybrid", cost: 2, x: 1190, y: 300 },
    { id: "hewn-stakes", title: "Тёсаные колья", description: "Топор готовит колья, а дубинка забивает их: открывается дешёвый частокол.", branch: "hybrid", cost: 2, x: 900, y: 550 },
    { id: "work-rhythm", title: "Ритм труда", description: "Удержание кнопки продолжает добычу; точные попадания ускоряют следующие замахи.", branch: "hybrid", cost: 3, x: 500, y: 80 },
    { id: "no-hesitation", title: "Без заминки", description: "Автосмена больше не прерывает ритм: новый инструмент входит прямо в следующий удар.", branch: "mastery", cost: 4, x: 560, y: 520 },
    { id: "resource-magnet", title: "Подхват на ходу", description: "Добытые куски сами летят к игроку, пока он продолжает работать или убегает.", branch: "mastery", cost: 3, x: 330, y: 790 },

    { id: "rough-foundation", title: "Грубый фундамент", description: "Открывает фундамент и платформу; призрак показывает будущую опору конструкции.", branch: "hybrid", cost: 2, x: 650, y: 930 },
    { id: "palisade", title: "Частокол", description: "Дешёвая линия защиты ломается секциями и ранит первого прорвавшегося врага.", branch: "axe", cost: 2, x: 900, y: 970 },
    { id: "spring-trap", title: "Пружинная ловушка", description: "Сбрасывает врага с платформы или разворачивает его обратно в толпу.", branch: "club", cost: 3, x: 1160, y: 930 },
    { id: "repair-beat", title: "Ритм ремонта", description: "Ремонт превращается в серию ударов; точный темп снижает расход материалов.", branch: "hybrid", cost: 3, x: 500, y: 1190 },
    { id: "area-blueprint", title: "Живой чертёж", description: "Протягивание создаёт план целой линии; ресурсы списываются только после подтверждения.", branch: "mastery", cost: 4, x: 760, y: 1360 },
    { id: "salvage-chain", title: "Разбор без потерь", description: "Последовательный демонтаж возвращает материалы цепочкой прямо в инвентарь.", branch: "mastery", cost: 4, x: 1010, y: 1250 },

    { id: "bomb-recipe", title: "Глиняная бомба", description: "Открывает бросаемую бомбу из камня и смолы с коротким видимым фитилём.", branch: "hybrid", cost: 3, x: 1370, y: 850 },
    { id: "powder-trail", title: "Пороховая дорожка", description: "Бомбы можно связать фитилём и поджечь всю подготовленную линию одним ударом.", branch: "mastery", cost: 4, x: 1580, y: 1080 },
    { id: "chain-reaction", title: "Цепная реакция", description: "Взрыв заряжает соседние ловушки и заставляет их сработать без задержки.", branch: "mastery", cost: 5, x: 1370, y: 1320 },
    { id: "sling", title: "Праща", description: "Первое дальнее оружие использует обычный камень и требует раскрутки перед броском.", branch: "club", cost: 3, x: 1500, y: 790 },
    { id: "crystal-shot", title: "Кристальный заряд", description: "Кристалл в праще рикошетит между отмеченными целями, но расходуется целиком.", branch: "hybrid", cost: 4, x: 1760, y: 900 },

    { id: "core-instinct", title: "Чутьё Ядра", description: "Перед волной Ядро показывает направление главного удара прямо на поверхности мира.", branch: "mastery", cost: 3, x: 1050, y: 1110 },
    { id: "last-light", title: "Последний свет", description: "При низком здоровье Ядра все незавершённые чертежи становятся временными баррикадами.", branch: "mastery", cost: 5, x: 1220, y: 1450 },
  ],
  edges: [
    { from: "spark", to: "axe" }, { from: "spark", to: "pickaxe" }, { from: "spark", to: "club" },
    { from: "axe", to: "clean-cut" }, { from: "axe", to: "falling-lesson" }, { from: "clean-cut", to: "resin-hunter" }, { from: "resin-hunter", to: "living-wood" },
    { from: "pickaxe", to: "weak-spots" }, { from: "pickaxe", to: "deep-vein" }, { from: "pickaxe", to: "echo-stone" }, { from: "weak-spots", to: "fault-line" }, { from: "echo-stone", to: "fault-line" },
    { from: "club", to: "wide-swing" }, { from: "club", to: "brace" }, { from: "wide-swing", to: "return-blow" }, { from: "brace", to: "return-blow" }, { from: "return-blow", to: "ground-pound" },
    { from: "axe", to: "right-tool" }, { from: "pickaxe", to: "right-tool" },
    { from: "pickaxe", to: "breaker" }, { from: "club", to: "breaker" },
    { from: "axe", to: "hewn-stakes" }, { from: "club", to: "hewn-stakes" },
    { from: "right-tool", to: "work-rhythm" }, { from: "right-tool", to: "no-hesitation" }, { from: "work-rhythm", to: "no-hesitation" }, { from: "work-rhythm", to: "resource-magnet" },
    { from: "right-tool", to: "rough-foundation" }, { from: "hewn-stakes", to: "palisade" }, { from: "hewn-stakes", to: "spring-trap" },
    { from: "rough-foundation", to: "repair-beat" }, { from: "palisade", to: "area-blueprint" }, { from: "repair-beat", to: "area-blueprint" }, { from: "area-blueprint", to: "salvage-chain" },
    { from: "resin-hunter", to: "bomb-recipe" }, { from: "deep-vein", to: "bomb-recipe" }, { from: "bomb-recipe", to: "powder-trail" }, { from: "spring-trap", to: "chain-reaction" }, { from: "powder-trail", to: "chain-reaction" },
    { from: "club", to: "sling" }, { from: "sling", to: "crystal-shot" }, { from: "deep-vein", to: "crystal-shot" },
    { from: "rough-foundation", to: "core-instinct" }, { from: "core-instinct", to: "last-light" }, { from: "salvage-chain", to: "last-light" },
  ],
  notes: ["Что игрок получает до первой ночи — только руки или ещё рецепт Ядра?", "Дубинка обязательна или её лучше заменить метательным камнем?", "Все входящие связи обязательны или гибрид может требовать любые две из трёх?", "Какие ноды покупаются навсегда, а какие выбираются только на текущий забег?"],
};

const STORAGE_KEY = "ian-skill-tree-designer-v2";
const NODE_W = 156;
const NODE_H = 156;

export default function Home() {
  const [tree, setTree] = useState<TreeState>(initialTree);
  const [selectedId, setSelectedId] = useState("spark");
  const [zoom, setZoom] = useState(0.86);
  const [snap, setSnap] = useState(true);
  const [saved, setSaved] = useState(true);
  const [noteDraft, setNoteDraft] = useState("");
  const canvasRef = useRef<HTMLDivElement>(null);
  const dragRef = useRef<{ id: string; dx: number; dy: number } | null>(null);

  useEffect(() => {
    const stored = localStorage.getItem(STORAGE_KEY);
    if (stored) {
      try { setTree(JSON.parse(stored) as TreeState); } catch { /* keep starter */ }
    }
  }, []);

  useEffect(() => {
    const timer = window.setTimeout(() => {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(tree));
      setSaved(true);
    }, 220);
    return () => window.clearTimeout(timer);
  }, [tree]);

  const selected = tree.nodes.find((node) => node.id === selectedId) ?? null;
  const nodeMap = useMemo(() => new Map(tree.nodes.map((node) => [node.id, node])), [tree.nodes]);

  const mutate = (fn: (current: TreeState) => TreeState) => {
    setSaved(false);
    setTree(fn);
  };

  const patchSelected = (patch: Partial<SkillNode>) => {
    if (!selected) return;
    mutate((current) => ({ ...current, nodes: current.nodes.map((node) => node.id === selected.id ? { ...node, ...patch } : node) }));
  };

  const beginDrag = (event: PointerEvent<HTMLButtonElement>, node: SkillNode) => {
    if (event.button !== 0) return;
    event.currentTarget.setPointerCapture(event.pointerId);
    dragRef.current = { id: node.id, dx: event.clientX / zoom - node.x, dy: event.clientY / zoom - node.y };
    setSelectedId(node.id);
  };

  const moveDrag = (event: PointerEvent<HTMLButtonElement>) => {
    const drag = dragRef.current;
    if (!drag) return;
    const bounds = canvasRef.current?.getBoundingClientRect();
    if (!bounds) return;
    let x = (event.clientX - bounds.left) / zoom - drag.dx + bounds.left / zoom;
    let y = (event.clientY - bounds.top) / zoom - drag.dy + bounds.top / zoom;
    x = Math.max(20, Math.min(1820, x));
    y = Math.max(20, Math.min(1460, y));
    if (snap) { x = Math.round(x / 20) * 20; y = Math.round(y / 20) * 20; }
    mutate((current) => ({ ...current, nodes: current.nodes.map((node) => node.id === drag.id ? { ...node, x, y } : node) }));
  };

  const addNode = () => {
    const id = `skill-${Date.now()}`;
    const node: SkillNode = { id, title: "Новая идея", description: "Опиши, как эта нода меняет поведение игрока.", branch: "hybrid", cost: 2, x: 900, y: 720 };
    mutate((current) => ({ ...current, nodes: [...current.nodes, node] }));
    setSelectedId(id);
  };

  const deleteSelected = () => {
    if (!selected || selected.id === "spark") return;
    mutate((current) => ({ ...current, nodes: current.nodes.filter((node) => node.id !== selected.id), edges: current.edges.filter((edge) => edge.from !== selected.id && edge.to !== selected.id) }));
    setSelectedId("spark");
  };

  const toggleDependency = (from: string) => {
    if (!selected || from === selected.id) return;
    const exists = tree.edges.some((edge) => edge.from === from && edge.to === selected.id);
    mutate((current) => ({ ...current, edges: exists ? current.edges.filter((edge) => !(edge.from === from && edge.to === selected.id)) : [...current.edges, { from, to: selected.id }] }));
  };

  const resetPositions = () => mutate((current) => ({ ...current, nodes: current.nodes.map((node) => {
    const original = initialTree.nodes.find((candidate) => candidate.id === node.id);
    return original ? { ...node, x: original.x, y: original.y } : node;
  }) }));

  const exportTree = () => {
    const blob = new Blob([JSON.stringify(tree, null, 2)], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement("a"); anchor.href = url; anchor.download = "skill-tree.json"; anchor.click(); URL.revokeObjectURL(url);
  };

  const importTree = (event: ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0]; if (!file) return;
    file.text().then((text) => { const imported = JSON.parse(text) as TreeState; mutate(() => imported); setSelectedId(imported.nodes[0]?.id ?? ""); });
    event.target.value = "";
  };

  return (
    <main className="app-shell">
      <header className="topbar">
        <div className="brand"><span className="brand-mark">✦</span><div><strong>Its Almost Night</strong><span>Skill Tree Lab</span></div></div>
        <div className="toolbar">
          <button onClick={addNode}>＋ Нода</button><button onClick={resetPositions}>Вернуть раскладку</button>
          <label className="import-button">Импорт<input type="file" accept="application/json" onChange={importTree} /></label>
          <button onClick={exportTree}>Экспорт JSON</button>
          <span className={`save-state ${saved ? "is-saved" : ""}`}>{saved ? "Сохранено" : "Сохраняю…"}</span>
        </div>
      </header>

      <aside className="left-panel panel">
        <div className="panel-heading"><span>Карта ветвей</span><small>{tree.nodes.length} нод</small></div>
        <p className="panel-copy">Свободный граф без уровней: ветки могут расти в любую сторону и снова соединяться.</p>
        <div className="legend">
          {(Object.keys(branchMeta) as Branch[]).map((branch) => <div key={branch}><i className={`dot ${branch}`} />{branchMeta[branch].label}<b>{tree.nodes.filter((node) => node.branch === branch).length}</b></div>)}
        </div>
        <div className="divider" />
        <div className="panel-heading"><span>Открытые вопросы</span><small>brainstorm</small></div>
        <div className="notes">
          {tree.notes.map((note, index) => <div className="note" key={`${note}-${index}`}><span>{note}</span><button aria-label="Удалить заметку" onClick={() => mutate((current) => ({ ...current, notes: current.notes.filter((_, i) => i !== index) }))}>×</button></div>)}
        </div>
        <form className="note-form" onSubmit={(event) => { event.preventDefault(); if (!noteDraft.trim()) return; mutate((current) => ({ ...current, notes: [...current.notes, noteDraft.trim()] })); setNoteDraft(""); }}>
          <input value={noteDraft} onChange={(event) => setNoteDraft(event.target.value)} placeholder="Добавить вопрос…" /><button>＋</button>
        </form>
      </aside>

      <section className="workspace">
        <div className="canvas-controls">
          <button onClick={() => setZoom((value) => Math.max(0.55, value - 0.1))}>−</button><span>{Math.round(zoom * 100)}%</span><button onClick={() => setZoom((value) => Math.min(1.2, value + 0.1))}>＋</button>
          <label><input type="checkbox" checked={snap} onChange={(event) => setSnap(event.target.checked)} /> Сетка</label>
        </div>
        <div className="canvas-viewport">
          <div className="tree-canvas" ref={canvasRef} style={{ transform: `scale(${zoom})` }}>
            <svg className="connections" viewBox="0 0 2000 1640" aria-hidden="true">
              {tree.edges.map((edge) => { const from = nodeMap.get(edge.from); const to = nodeMap.get(edge.to); if (!from || !to) return null; const x1 = from.x + NODE_W / 2; const y1 = from.y + NODE_H / 2; const x2 = to.x + NODE_W / 2; const y2 = to.y + NODE_H / 2; return <g key={`${edge.from}-${edge.to}`}><line className="connection-shadow" x1={x1} y1={y1} x2={x2} y2={y2} /><line className={`connection ${to.branch}`} x1={x1} y1={y1} x2={x2} y2={y2} /></g>; })}
            </svg>
            {tree.nodes.map((node) => <button key={node.id} className={`skill-node ${node.branch} ${selectedId === node.id ? "selected" : ""}`} style={{ left: node.x, top: node.y }} onPointerDown={(event) => beginDrag(event, node)} onPointerMove={moveDrag} onPointerUp={() => { dragRef.current = null; }} onClick={() => setSelectedId(node.id)}>
              <span className="node-icon">{branchMeta[node.branch].icon}</span><span className="node-body"><small>{branchMeta[node.branch].label} · {node.cost} ✦</small><strong>{node.title}</strong><em>{node.description}</em></span>
            </button>)}
          </div>
        </div>
      </section>

      <aside className="right-panel panel">
        {selected ? <>
          <div className="panel-heading"><span>Инспектор</span><small>{branchMeta[selected.branch].label}</small></div>
          <label className="field"><span>Название</span><input value={selected.title} onChange={(event) => patchSelected({ title: event.target.value })} /></label>
          <label className="field"><span>Что меняется</span><textarea value={selected.description} onChange={(event) => patchSelected({ description: event.target.value })} /></label>
          <div className="field-row"><label className="field"><span>Ветка</span><select value={selected.branch} onChange={(event) => patchSelected({ branch: event.target.value as Branch })}>{(Object.keys(branchMeta) as Branch[]).map((branch) => <option value={branch} key={branch}>{branchMeta[branch].label}</option>)}</select></label><label className="field"><span>Цена</span><input type="number" min="0" max="20" value={selected.cost} onChange={(event) => patchSelected({ cost: Number(event.target.value) })} /></label></div>
          <div className="divider" />
          <div className="panel-heading"><span>Требует</span><small>{tree.edges.filter((edge) => edge.to === selected.id).length} связей</small></div>
          <div className="dependency-list">{tree.nodes.filter((node) => node.id !== selected.id).map((node) => { const active = tree.edges.some((edge) => edge.from === node.id && edge.to === selected.id); return <button className={active ? "active" : ""} key={node.id} onClick={() => toggleDependency(node.id)}><i />{node.title}<span>{active ? "связано" : "＋"}</span></button>; })}</div>
          <button className="danger" disabled={selected.id === "spark"} onClick={deleteSelected}>Удалить ноду</button>
        </> : <div className="empty-inspector">Выбери ноду на холсте</div>}
      </aside>
    </main>
  );
}
