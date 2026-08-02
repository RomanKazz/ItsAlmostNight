"use client";

import { ChangeEvent, PointerEvent, useEffect, useMemo, useRef, useState } from "react";

type Branch = "root" | "axe" | "pickaxe" | "club" | "hybrid" | "mastery";
type SkillNode = {
  id: string;
  title: string;
  description: string;
  branch: Branch;
  tier: number;
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
    { id: "spark", title: "Искра ремесла", description: "После первой пережитой ночи открывает выбор первого настоящего инструмента.", branch: "root", tier: 0, cost: 1, x: 510, y: 790 },
    { id: "axe", title: "Каменный топор", description: "Рубка деревьев становится отдельным быстрым действием.", branch: "axe", tier: 1, cost: 1, x: 230, y: 610 },
    { id: "pickaxe", title: "Каменная кирка", description: "Позволяет эффективно разбивать камень и кристаллы.", branch: "pickaxe", tier: 1, cost: 1, x: 510, y: 610 },
    { id: "club", title: "Тяжёлая дубинка", description: "Первое оружие: медленное, грубое и способное отбрасывать.", branch: "club", tier: 1, cost: 1, x: 790, y: 610 },
    { id: "right-tool", title: "Верный инструмент", description: "Топор и кирка автоматически сменяются при наведении на подходящий ресурс.", branch: "hybrid", tier: 2, cost: 2, x: 370, y: 400 },
    { id: "breaker", title: "Рабочий замах", description: "Добывающий инструмент можно использовать в бою; точное попадание оглушает.", branch: "hybrid", tier: 2, cost: 2, x: 650, y: 400 },
    { id: "rhythm", title: "Ритм труда", description: "Удержание кнопки продолжает добычу, но промах сбрасывает набранный темп.", branch: "mastery", tier: 3, cost: 3, x: 370, y: 190 },
    { id: "improvise", title: "Импровизатор", description: "Последний использованный инструмент получает особое добивание по своей цели.", branch: "mastery", tier: 3, cost: 3, x: 650, y: 190 },
    { id: "survivalist", title: "Хозяин ночи", description: "Открывает продвинутые варианты инструментов и связанные с ними сооружения.", branch: "mastery", tier: 4, cost: 5, x: 510, y: 32 },
  ],
  edges: [
    { from: "spark", to: "axe" }, { from: "spark", to: "pickaxe" }, { from: "spark", to: "club" },
    { from: "axe", to: "right-tool" }, { from: "pickaxe", to: "right-tool" },
    { from: "pickaxe", to: "breaker" }, { from: "club", to: "breaker" },
    { from: "right-tool", to: "rhythm" }, { from: "right-tool", to: "improvise" },
    { from: "breaker", to: "improvise" }, { from: "rhythm", to: "survivalist" }, { from: "improvise", to: "survivalist" },
  ],
  notes: ["Что игрок получает до первой ночи — только руки или ещё рецепт Ядра?", "Дубинка обязательна или её лучше заменить метательным камнем?"],
};

const STORAGE_KEY = "ian-skill-tree-designer-v1";
const NODE_W = 210;
const NODE_H = 104;

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
    x = Math.max(20, Math.min(1000, x));
    y = Math.max(20, Math.min(820, y));
    if (snap) { x = Math.round(x / 20) * 20; y = Math.round(y / 20) * 20; }
    mutate((current) => ({ ...current, nodes: current.nodes.map((node) => node.id === drag.id ? { ...node, x, y } : node) }));
  };

  const addNode = () => {
    const id = `skill-${Date.now()}`;
    const node: SkillNode = { id, title: "Новая идея", description: "Опиши, как эта нода меняет поведение игрока.", branch: "hybrid", tier: 2, cost: 2, x: 510, y: 400 };
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

  const autoLayout = () => {
    const tiers = new Map<number, SkillNode[]>();
    tree.nodes.forEach((node) => tiers.set(node.tier, [...(tiers.get(node.tier) ?? []), node]));
    const ys = [790, 610, 400, 190, 32];
    mutate((current) => ({ ...current, nodes: current.nodes.map((node) => {
      const row = tiers.get(node.tier) ?? [node];
      const index = row.findIndex((item) => item.id === node.id);
      const gap = 980 / (row.length + 1);
      return { ...node, x: Math.round(gap * (index + 1)), y: ys[node.tier] ?? Math.max(20, 790 - node.tier * 190) };
    }) }));
  };

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
          <button onClick={addNode}>＋ Нода</button><button onClick={autoLayout}>Выровнять</button>
          <label className="import-button">Импорт<input type="file" accept="application/json" onChange={importTree} /></label>
          <button onClick={exportTree}>Экспорт JSON</button>
          <span className={`save-state ${saved ? "is-saved" : ""}`}>{saved ? "Сохранено" : "Сохраняю…"}</span>
        </div>
      </header>

      <aside className="left-panel panel">
        <div className="panel-heading"><span>Карта ветвей</span><small>{tree.nodes.length} нод</small></div>
        <p className="panel-copy">Дерево растёт снизу вверх. Гибридная нода требует все входящие связи.</p>
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
            {[4, 3, 2, 1, 0].map((tier) => <div key={tier} className="tier-line" style={{ top: [842, 662, 452, 242, 84][tier] }}><span>УРОВЕНЬ {tier}</span></div>)}
            <svg className="connections" viewBox="0 0 1240 940" aria-hidden="true">
              {tree.edges.map((edge) => { const from = nodeMap.get(edge.from); const to = nodeMap.get(edge.to); if (!from || !to) return null; const x1 = from.x + NODE_W / 2; const y1 = from.y; const x2 = to.x + NODE_W / 2; const y2 = to.y + NODE_H; return <g key={`${edge.from}-${edge.to}`}><line className="connection-shadow" x1={x1} y1={y1} x2={x2} y2={y2} /><line className={`connection ${to.branch}`} x1={x1} y1={y1} x2={x2} y2={y2} /></g>; })}
            </svg>
            {tree.nodes.map((node) => <button key={node.id} className={`skill-node ${node.branch} ${selectedId === node.id ? "selected" : ""}`} style={{ left: node.x, top: node.y }} onPointerDown={(event) => beginDrag(event, node)} onPointerMove={moveDrag} onPointerUp={() => { dragRef.current = null; }} onClick={() => setSelectedId(node.id)}>
              <span className="node-icon">{branchMeta[node.branch].icon}</span><span className="node-body"><small>{branchMeta[node.branch].label} · {node.cost} ✦</small><strong>{node.title}</strong><em>{node.description}</em></span>
            </button>)}
          </div>
        </div>
      </section>

      <aside className="right-panel panel">
        {selected ? <>
          <div className="panel-heading"><span>Инспектор</span><small>уровень {selected.tier}</small></div>
          <label className="field"><span>Название</span><input value={selected.title} onChange={(event) => patchSelected({ title: event.target.value })} /></label>
          <label className="field"><span>Что меняется</span><textarea value={selected.description} onChange={(event) => patchSelected({ description: event.target.value })} /></label>
          <div className="field-row"><label className="field"><span>Ветка</span><select value={selected.branch} onChange={(event) => patchSelected({ branch: event.target.value as Branch })}>{(Object.keys(branchMeta) as Branch[]).map((branch) => <option value={branch} key={branch}>{branchMeta[branch].label}</option>)}</select></label><label className="field"><span>Цена</span><input type="number" min="0" max="20" value={selected.cost} onChange={(event) => patchSelected({ cost: Number(event.target.value) })} /></label></div>
          <label className="field"><span>Уровень</span><input type="range" min="0" max="4" value={selected.tier} onChange={(event) => patchSelected({ tier: Number(event.target.value) })} /><b className="range-value">{selected.tier}</b></label>
          <div className="divider" />
          <div className="panel-heading"><span>Требует</span><small>{tree.edges.filter((edge) => edge.to === selected.id).length} связей</small></div>
          <div className="dependency-list">{tree.nodes.filter((node) => node.id !== selected.id && node.tier < selected.tier).map((node) => { const active = tree.edges.some((edge) => edge.from === node.id && edge.to === selected.id); return <button className={active ? "active" : ""} key={node.id} onClick={() => toggleDependency(node.id)}><i />{node.title}<span>{active ? "связано" : "＋"}</span></button>; })}</div>
          <button className="danger" disabled={selected.id === "spark"} onClick={deleteSelected}>Удалить ноду</button>
        </> : <div className="empty-inspector">Выбери ноду на холсте</div>}
      </aside>
    </main>
  );
}
