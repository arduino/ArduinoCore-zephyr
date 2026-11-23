// Copyright (c) Arduino s.r.l. and/or its affiliated companies
// SPDX-License-Identifier: Apache-2.0

// Renderer of the CI report page (see ci_report_page.py). The render
// functions are pure: they return HTML strings from the report data and the
// current view, so they can be tested with node. The DOM layer at the end
// only runs in the browser.

'use strict';

(function (root) {

const esc = (s) => String(s ?? '').replace(/[&<>"']/g,
  (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);

// --- data model

// sketch identity: the core tests name the sketches from the parent folder
// ('ArduinoCore-zephyr/libraries/...'), the native_sim tests from the core
// ('libraries/...')
const sketchKey = (sketch) => String(sketch ?? '').replace(/^ArduinoCore-zephyr\//, '');

function buildModel(p) {
  const statuses = new Map(p.statuses.map((s) => [s.value, s]));
  const byName = Object.fromEntries(p.statuses.map((s) => [s.name, s.value]));

  const boardsByArtifact = new Map(p.run.artifacts.map((a) => [a, new Set()]));
  const add = (artifact, board) => {
    if (!boardsByArtifact.has(artifact)) boardsByArtifact.set(artifact, new Set());
    boardsByArtifact.get(artifact).add(board);
  };
  for (const t of p.tests) add(t.artifact, t.board);
  for (const [board, l] of Object.entries(p.loaders)) add(l.artifact, board);
  for (const [a, set] of boardsByArtifact) boardsByArtifact.set(a, [...set].sort());

  // native_sim last (its data is appended to the core artifacts)
  const artifacts = [...boardsByArtifact.keys()].sort((a, b) => (a === 'native_sim') - (b === 'native_sim'));

  // test groups (library or examples group, '' for the CI entries) and their sketches
  const groups = new Map();
  for (const t of p.tests) {
    if (!groups.has(t.group)) groups.set(t.group, new Set());
    groups.get(t.group).add(sketchKey(t.sketch));
  }
  for (const [g, set] of groups) groups.set(g, [...set].sort());

  // SoCs and their boards (native_sim and the boards it poses as: 'native')
  const socs = new Map();
  for (const b of [...boardsByArtifact.values()].flat()) {
    const soc = p.boards[b]?.soc;
    if (!soc) continue;
    if (!socs.has(soc)) socs.set(soc, []);
    socs.get(soc).push(b);
  }
  for (const list of socs.values()) list.sort();

  // sketches run on native_sim (false: some run checks failed), by board and
  // sketch: on the native_sim boards and on the boards they pose as
  const runs = new Map();
  const addRun = (board, sketch, ok) => {
    const k = `${board}\n${sketchKey(sketch)}`;
    runs.set(k, (runs.get(k) ?? true) && ok);
  };
  for (const t of p.tests) {
    if (typeof t.run_success !== 'boolean') continue;
    addRun(t.board, t.sketch, t.run_success);
    const mimic = p.boards[t.board]?.mimic;
    if (mimic) addRun(mimic, t.sketch, t.run_success);
  }

  return {
    p, statuses, S: byName,
    artifacts,
    // page of the native_sim tests only (see test_native_sim.yml)
    nativeOnly: p.run.artifacts.length === 1 && p.run.artifacts[0] === 'native_sim',
    boardsByArtifact,
    groups: new Map([...groups].sort(([a], [b]) => a.localeCompare(b))),
    socs: new Map([...socs].sort(([a], [b]) => a.localeCompare(b))),
    status: (value) => statuses.get(value) ?? statuses.get(byName.SKIP),
    label: (board) => (p.boards[board]?.mimic ? `${p.boards[board].mimic} (sim)` : board),
    sketchLabel: (t) => (t.group ? `${t.group} ${t.name}` : t.name),
    // native_sim run of a sketch on a board: true, false, or undefined (not run)
    ran: (board, sketch) => runs.get(`${board}\n${sketchKey(sketch)}`),
  };
}

// boards of a board selection (artifact, board, SoC), null for all
function selectionBoards(m, sel) {
  if (!sel || sel.kind === 'all') return null;
  if (sel.kind === 'artifact') return m.boardsByArtifact.get(sel.id) ?? [];
  if (sel.kind === 'soc') return m.socs.get(sel.id) ?? [];
  return [sel.id];
}

// tests of the board selection (all, artifact, board, SoC) and of the test
// selection (all, group, sketch); either side can be ignored
function selectTests(m, view, { boards = true, tests = true } = {}) {
  const b = boards && selectionBoards(m, view.boards);
  const inBoards = b && new Set(b);
  const s = view.tests ?? { kind: 'all' };
  return m.p.tests.filter((t) =>
    (!inBoards || inBoards.has(t.board))
    && (!tests || s.kind === 'all' || (s.kind === 'group' ? t.group === s.id : sketchKey(t.sketch) === s.id)));
}

// badge of a set of tests and boards: the first that applies of a failed
// loader or CI test (failure), issues, known failures, warnings, only
// skipped tests, clean
function badge(m, tests, boards = []) {
  const S = m.S;
  const n = (...values) => tests.filter((t) => values.includes(t.status)).length;
  const make = (kind, status, count) => ({ kind, icon: m.status(status).test_icon, count });
  const failed = n(S.FAILURE) + boards.filter((b) => m.p.loaders[b]?.status === S.FAILURE).length;
  if (failed) return make('failure', S.FAILURE, failed);
  if (n(S.INVALID_EXCEPTION, S.ERROR)) return make('issues', S.ERROR, n(S.INVALID_EXCEPTION, S.ERROR));
  if (n(S.EXPECTED_ERROR)) return make('known', S.EXPECTED_ERROR, n(S.EXPECTED_ERROR));
  if (n(S.WARNING)) return make('warnings', S.WARNING, n(S.WARNING));
  if (tests.every((t) => t.status === S.SKIP)) return make('skipped', S.SKIP, 0);
  return make('clean', S.PASS, 0);
}

// --- issue grouping (cards of the Test issues tab)

// text that identifies an error: the issue lines, without the arduino-cli
// incompatibility warnings and the 'Used library/platform' trailer (paths
// differ per board); the collected issues already elide the path prefixes
const INCOMPATIBLE_RE = /^WARNING: library .* may be incompatible/;
function issueKey(issues) {
  const lines = [];
  for (const line of issues.join('\n').split('\n')) {
    if (/^Used (library|platform)\b/.test(line)) break;
    if (!INCOMPATIBLE_RE.test(line)) lines.push(line);
  }
  while (lines.length && !lines.at(-1).trim()) lines.pop();
  return lines.join('\n');
}

// cards of each status group: one card per distinct error, with the
// sketches and the (board, link mode) jobs it hits; passed tests by sketch,
// skipped tests by reason
function cardGroups(m, tests) {
  const S = m.S;
  const groupOf = (status) => (status >= S.INVALID_EXCEPTION ? 'issues'
    : status === S.EXPECTED_ERROR ? 'known' : status === S.WARNING ? 'warnings' : null);
  const cards = { issues: new Map(), known: new Map(), warnings: new Map() };
  const passed = new Map();
  const skipped = new Map();
  for (const t of tests) {
    const g = groupOf(t.status);
    if (g) {
      const text = issueKey(t.issues);
      const key = text || `\u0000${sketchKey(t.sketch)}`;
      if (!cards[g].has(key)) cards[g].set(key, { status: t.status, key, text, sketches: new Set(), hits: [] });
      const card = cards[g].get(key);
      card.status = Math.max(card.status, t.status);
      card.sketches.add(sketchKey(t.sketch));
      card.hits.push({ board: t.board, link_mode: t.link_mode, status: t.status, job_link: t.job_link,
                       sketch: t.sketch, raw: t.issues.join('\n') });
    } else if (t.status === S.SKIP) {
      const reason = t.issues.join('\n');
      const k = `${reason}\n${sketchKey(t.sketch)}`;
      if (!skipped.has(k)) skipped.set(k, { reason, sketch: sketchKey(t.sketch), label: m.sketchLabel(t), boards: new Set() });
      skipped.get(k).boards.add(t.board);
    } else {
      const k = sketchKey(t.sketch);
      if (!passed.has(k)) passed.set(k, { sketch: k, label: m.sketchLabel(t), boards: [] });
      passed.get(k).boards.push({ board: t.board, link_mode: t.link_mode });
    }
  }
  const finish = (map) => [...map.values()].map((c) => ({
    ...c,
    sketches: [...c.sketches].sort(),
    hits: c.hits.sort((a, b) => m.label(a.board).localeCompare(m.label(b.board)) || a.link_mode.localeCompare(b.link_mode)
      || a.sketch.localeCompare(b.sketch)),
  })).sort((a, b) => b.status - a.status || b.hits.length - a.hits.length || a.sketches[0].localeCompare(b.sketches[0]));
  return {
    issues: finish(cards.issues),
    known: finish(cards.known),
    warnings: finish(cards.warnings),
    passed: [...passed.values()].sort((a, b) => a.label.localeCompare(b.label)),
    skipped: [...skipped.values()].map((r) => ({ ...r, boards: [...r.boards].sort() }))
      .sort((a, b) => a.reason.localeCompare(b.reason) || a.label.localeCompare(b.label)),
  };
}

// --- helpers

// case-insensitive substring match, no pattern characters
const matches = (text, filter) => !filter || String(text).toLowerCase().includes(filter.toLowerCase());

// escaped text, with the occurrences of the filter in <mark>
function hl(text, filter) {
  const s = String(text ?? '');
  if (!filter) return esc(s);
  const lower = s.toLowerCase();
  const f = filter.toLowerCase();
  let out = '';
  let i = 0;
  for (let j = lower.indexOf(f); j >= 0; j = lower.indexOf(f, i)) {
    out += `${esc(s.slice(i, j))}<mark>${esc(s.slice(j, j + f.length))}</mark>`;
    i = j + f.length;
  }
  return out + esc(s.slice(i));
}

const code = (s) => `<code>${esc(s)}</code>`;
const link = (href, text) => (href ? `<a href='${esc(href)}'>${text}</a>` : text);
const isNum = (v) => typeof v === 'number' && Number.isFinite(v);
const pct = (used, max) => (isNum(used) && isNum(max) && max > 0 ? `${(100 * used / max).toFixed(1)}%` : '–');
const bar = (used, max) => (isNum(used) && isNum(max) && max > 0
  ? `<span class='bar'><i class='${used / max > 0.85 ? 'hi' : ''}' style='width:${Math.min(100, (100 * used / max)).toFixed(0)}%'></i></span>` : '');
const signed = (n) => (n > 0 ? `+${n}` : `${n}`);
const groupLabel = (g) => (g === '' ? '(CI)' : g);
const testText = (m, t) => [t.artifact, m.label(t.board), t.board, t.link_mode, t.sketch, m.sketchLabel(t),
  m.status(t.status).name, ...t.issues].join('\n');
// 'NAME=value' texts of a Kconfig option (y is stored as 1: match both)
const kconfigTexts = (name, value) => (value === 1 ? [`${name}=1`, `${name}=y`] : [`${name}=${value}`]);
const loaderText = (m, b) => {
  const l = m.p.loaders[b] ?? {};
  return [m.label(b), b, ...(l.warnings ?? []), ...Object.entries(l.config ?? {}).flatMap(([n, v]) => kconfigTexts(n, v))].join('\n');
};

function table(head, rows, cls = '') {
  if (!rows.length) return '';
  return `<div class='scroll'><table${cls ? ` class='${cls}'` : ''}><thead><tr>${head.map((h) => `<th>${h}</th>`).join('')}</tr></thead><tbody>${rows.join('')}</tbody></table></div>`;
}

// with native_sim collapsed in the recap, the boards it poses as are hidden
// where the board they pose as is also listed (that carries their run marks)
function hideTwins(m, view, boards) {
  if (recapOpen(m, view, 'native_sim')) return boards;
  const set = new Set(boards);
  return boards.filter((b) => !set.has(m.p.boards[b]?.mimic));
}

// icon of the native_sim run of some sketches on a board: failed if any failed
function ranIcon(m, board, sketches) {
  const r = sketches.map((s) => m.ran(board, s)).filter((v) => v !== undefined);
  return !r.length ? '' : r.includes(false) ? ` ${m.p.icons.run_failed}` : ` ${m.p.icons.ran}`;
}
const note = (text) => `<p class='note'>${text}</p>`;

// state of a <details> section: the user's choice (view.open has 'key' when
// opened, '!key' when closed), else the default
function isOpen(view, key, def) {
  if (view.open?.has(key)) return true;
  if (view.open?.has(`!${key}`)) return false;
  return def;
}

function badgeHtml(b) {
  if (!b) return '';
  return `<span class='badge b-${b.kind}'>${b.icon}${b.count ? ` ${b.count}` : ''}</span>`;
}

// boards of the board selection; with a test selection, only the boards
// that ran those tests
function selectedBoards(m, view) {
  const boards = selectionBoards(m, view.boards) ?? m.artifacts.flatMap((a) => m.boardsByArtifact.get(a));
  if ((view.tests ?? { kind: 'all' }).kind === 'all') return boards;
  const ran = new Set(selectTests(m, view).map((t) => t.board));
  return boards.filter((x) => ran.has(x));
}

// tests of both selections and of the text filter
const filteredTests = (m, view) => selectTests(m, view).filter((t) => matches(testText(m, t), view.filter));

function selectionLabel(m, sel) {
  if (sel.kind === 'board') return m.label(sel.id);
  if (sel.kind === 'sketch') {
    const t = m.p.tests.find((x) => sketchKey(x.sketch) === sel.id);
    return t ? m.sketchLabel(t) : sel.id;
  }
  return sel.kind === 'group' ? groupLabel(sel.id) : sel.id;
}

// --- sidebar

function entry(m, view, side, kind, id, text, b, dim, toggle = '') {
  const cur = view[side] ?? { kind: 'all' };
  const selected = cur.kind === kind && (kind === 'all' || cur.id === id);
  const cls = [kind === 'all' || kind === 'artifact' || kind === 'group' ? 'top' : kind === 'soc' ? '' : 'sub', selected ? 'selected' : '', dim ? 'dim' : '']
    .filter(Boolean).join(' ');
  return `<div class='row'>${toggle}<button class='${cls}' data-sel='${side}' data-kind='${kind}' data-id='${esc(id)}'>`
    + `<span class='name'>${esc(text)}</span>${badgeHtml(b)}</button></div>`;
}

const toggleBtn = (key, expanded) => `<button class='toggle' data-toggle='${esc(key)}' aria-label='expand'>${expanded ? '▾' : '▸'}</button>`;

function renderSidebar(m, view) {
  const filter = view.filter ?? '';
  // badges of each side are counted on the tests of the other selection
  const forBoards = selectTests(m, view, { boards: false });
  // (the known failures of native_sim count as passed on the tests side,
  // unless only native_sim boards are selected)
  const simBoards = new Set(m.boardsByArtifact.get('native_sim') ?? []);
  const selBoards = selectionBoards(m, view.boards ?? { kind: 'all' });
  const simOnly = m.nativeOnly || (selBoards?.length > 0 && selBoards.every((b) => simBoards.has(b)));
  const forTests = selectTests(m, view, { tests: false }).map((t) => (!simOnly && t.artifact === 'native_sim'
    && t.status === m.S.EXPECTED_ERROR ? { ...t, status: m.S.PASS } : t));
  const hit = (tests, boards = []) => !filter || tests.some((t) => matches(testText(m, t), filter))
    || boards.some((b) => matches(loaderText(m, b), filter));
  const expanded = (key, def) => (view.collapsed?.has(key) ? !def : def);
  // entries with no tests in the other side's selection: dimmed, no badge
  const allTests = (view.tests ?? { kind: 'all' }).kind === 'all';
  const allBoards = (view.boards ?? { kind: 'all' }).kind === 'all';
  const boardEntry = (kind, id, text, tests, list, toggle) => (tests.length || allTests
    ? entry(m, view, 'boards', kind, id, text, badge(m, tests, list), !hit(tests, list), toggle)
    : entry(m, view, 'boards', kind, id, text, null, true, toggle));
  const testEntry = (kind, id, text, tests, toggle) => (tests.length || allBoards
    ? entry(m, view, 'tests', kind, id, text, badge(m, tests), !hit(tests), toggle)
    : entry(m, view, 'tests', kind, id, text, null, true, toggle));

  const boards = [entry(m, view, 'boards', 'all', '', 'All', badge(m, forBoards, Object.keys(m.p.loaders)), false)];
  for (const a of m.artifacts) {
    const list = m.boardsByArtifact.get(a);
    const tests = forBoards.filter((t) => t.artifact === a);
    const key = `artifact:${a}`;
    const open = expanded(key, a !== 'native_sim' || m.nativeOnly);
    boards.push(boardEntry('artifact', a, a, tests, list, toggleBtn(key, open)));
    if (!open) continue;
    for (const b of list) {
      const bt = tests.filter((t) => t.board === b);
      boards.push(boardEntry('board', b, m.label(b), bt, [b]));
    }
  }

  // SoCs: another way to select boards
  const socs = [entry(m, view, 'boards', 'all', '', 'All', badge(m, forBoards, Object.keys(m.p.loaders)), false),
    ...[...m.socs].map(([soc, list]) => boardEntry('soc', soc, soc, forBoards.filter((t) => list.includes(t.board)), list))];

  const tests = [entry(m, view, 'tests', 'all', '', 'All', badge(m, forTests), false)];
  const cur = view.tests ?? { kind: 'all' };
  for (const [g, sketches] of m.groups) {
    const gt = forTests.filter((t) => t.group === g);
    const key = `group:${g}`;
    const open = expanded(key, false) || (cur.kind === 'sketch' && sketches.includes(cur.id));
    tests.push(testEntry('group', g, groupLabel(g), gt, toggleBtn(key, open)));
    if (!open) continue;
    for (const s of sketches) {
      const st = gt.filter((t) => sketchKey(t.sketch) === s);
      const name = m.p.tests.find((t) => sketchKey(t.sketch) === s)?.name ?? s;
      tests.push(testEntry('sketch', s, name, st));
    }
  }

  return `<input id='filter' type='search' placeholder='filter text…' autocomplete='off' value='${esc(view.filterRaw ?? filter)}'${filter ? " class='active'" : ''}>`
    + `<div class='panel'><h3>BOARDS</h3>${boards.join('')}</div>`
    + `<div class='panel'><h3>SOCS</h3>${socs.join('')}</div>`
    + `<div class='panel'><h3>TESTS</h3>${tests.join('')}</div>`
    + renderLegend(m);
}

// the icons of the statuses (test icon, and board icon when different) and
// of the native_sim runs
function renderLegend(m) {
  const row = (icons, text) => `<div class='lrow'><span class='licons'>${icons}</span><span>${esc(text)}</span></div>`;
  const rows = m.p.statuses.map((s) => row(s.board_icon === s.test_icon ? s.test_icon : `${s.test_icon} ${s.board_icon}`, s.legend));
  rows.push(row(m.p.icons.ran, 'Sketch also ran on native_sim, and all its run checks passed.'));
  rows.push(row(m.p.icons.run_failed, 'Sketch also ran on native_sim, and some run checks failed.'));
  return `<div class='panel legend'><h3>LEGEND</h3>${rows.join('')}</div>`;
}

// --- filter bar, header, recap

function renderFilterBar(m, view) {
  const chips = [];
  const chip = (what, text, clear) => chips.push(`<span class='fchip' title='${esc(text.replace(/^"|"$/g, ''))}'><b>${what}:</b> ${esc(text)} <button data-clear='${clear}' title='remove'>×</button></span>`);
  if (view.boards && view.boards.kind !== 'all') chip(view.boards.kind === 'soc' ? 'SoC' : 'Boards', selectionLabel(m, view.boards), 'boards');
  if (view.tests && view.tests.kind !== 'all') chip('Tests', selectionLabel(m, view.tests), 'tests');
  if (view.filter) chip('Text', `"${view.filter}"`, 'filter');
  const tests = filteredTests(m, view);
  const issues = tests.filter((t) => t.status >= m.S.INVALID_EXCEPTION).length;
  const counts = `${new Set(tests.map((t) => t.board)).size} boards · ${tests.length} tests · ${m.status(m.S.ERROR).test_icon} ${issues}`;
  return `<div id='filterbar'><span class='lbl'>Showing${chips.length ? '' : ' all'}</span>${chips.join('')}`
    + `<span class='cnt'>${counts}</span>${chips.length ? "<button class='clr' data-clear='all'>Clear all</button>" : ''}</div>`;
}

const EVENTS = { push: 'push', pull_request: 'pull request', pull_request_target: 'pull request',
  workflow_dispatch: 'manual run', schedule: 'scheduled run', merge_group: 'merge queue', release: 'release' };

// the information on the CI run (see run_info() in ci_report_page.py):
// pull request or commit title, then repository, branch, commit, trigger,
// workflow run (linked) and date
function runInfo(info, runLink) {
  if (!info || !Object.keys(info).length) return '';
  let title = '';
  if (info.pr_number) title = `${link(info.pr_url, `#${esc(info.pr_number)}`)} ${esc(info.title ?? '')}`;
  else if (info.title) title = esc(info.title);
  const parts = [];
  if (info.repository) parts.push(link(info.repository_url, esc(info.repository)));
  if (info.branch) parts.push(link(info.branch_url, code(info.branch)) + (info.base ? ` → ${code(info.base)}` : ''));
  if (info.sha) parts.push(link(info.commit_url, code(info.sha.slice(0, 8))));
  if (info.event) parts.push(`${esc(EVENTS[info.event] ?? info.event)}${info.actor ? ` by ${esc(info.actor)}` : ''}`);
  if (info.run_number) parts.push(`${esc(info.workflow ?? 'run')} ${link(runLink, `#${esc(info.run_number)}`)}`
    + `${info.run_attempt && info.run_attempt !== '1' ? ` (attempt ${esc(info.run_attempt)})` : ''}`);
  if (info.date) parts.push(esc(info.date.replace('T', ' ').replace(/:\d\dZ$/, ' UTC')));
  return `<div class='runinfo'>${title ? `<div class='title'>${title}</div>` : ''}`
    + `${parts.length ? `<div class='meta'>${parts.join(' · ')}</div>` : ''}</div>`;
}

function renderHeader(m) {
  const p = m.p;
  const run = p.run.ci_run_passed ? `PASSED ${m.status(m.S.PASS).test_icon}` : `FAILED ${m.status(m.S.ERROR).test_icon}`;
  const ns = p.native_sim.present ? '' : `${m.status(m.S.FAILURE).test_icon} native_sim: not built`;
  // the workflow run is linked from its number, if known
  const notes = [p.info?.run_number ? '' : link(p.run.job_link_stem, 'workflow run ↗'), ns].filter(Boolean);
  return `<header><h1>${m.nativeOnly ? 'native_sim ' : ''}CI run — ${run}</h1>${runInfo(p.info, p.run.job_link_stem)}`
    + `${notes.length ? `<div class='note'>${notes.join(' · ')}</div>` : ''}</header>`;
}

const pageTitle = (m) => `${m.nativeOnly ? 'native_sim ' : ''}CI report: ${m.p.run.ci_run_passed ? 'PASSED' : 'FAILED'}`;

function results(m, tests) {
  const S = m.S;
  // one outcome per sketch of each board, not per link mode test: a sketch
  // built for both link modes counts once, with its worst status. A sketch
  // that also ran on native_sim counts with its run icon instead of its
  // status when the run replaces it: passed, or failed with an error
  const outcomes = new Map();
  for (const t of tests) {
    const k = `${t.board}\n${sketchKey(t.sketch)}`;
    const status = Math.max(outcomes.get(k)?.status ?? S.SKIP, t.status);
    outcomes.set(k, { status, ran: m.ran(t.board, t.sketch) });
  }
  const kinds = [...outcomes.values()].map(({ status, ran }) => {
    if (status === S.PASS && ran === true) return 'ran';
    if ((status === S.ERROR || status === S.INVALID_EXCEPTION) && ran === false) return 'run_failed';
    return status;
  });
  const n = (...v) => kinds.filter((k) => v.includes(k)).length;
  const parts = [`${kinds.length} sketches`];
  const add = (count, icon) => { if (count) parts.push(`${count} ${icon}`); };
  add(n(S.FAILURE), m.status(S.FAILURE).test_icon);
  add(n(S.INVALID_EXCEPTION, S.ERROR), m.status(S.ERROR).test_icon);
  add(n('run_failed'), m.p.icons.run_failed);
  add(n(S.EXPECTED_ERROR), m.status(S.EXPECTED_ERROR).test_icon);
  add(n(S.WARNING), m.status(S.WARNING).test_icon);
  add(n(S.PASS), m.status(S.PASS).test_icon);
  add(n('ran'), m.p.icons.ran);
  add(n(S.SKIP), m.status(S.SKIP).test_icon);
  return parts.join(', ');
}

function recapRow(m, view, b, tests) {
  const l = m.p.loaders[b];
  const S = m.S;
  const logs = [...new Map(m.p.tests.filter((t) => t.board === b && t.job_link).map((t) => [t.link_mode || 'log', t.job_link]))]
    .sort().map(([mode, href]) => link(href, esc(mode)));
  if (l?.job_link) logs.push(link(l.job_link, 'loader'));
  const name = `<td><button class='blink' data-sel='boards' data-kind='board' data-id='${esc(b)}'>${code(m.label(b))}</button></td>`;
  if (l?.status === S.FAILURE) {
    return `<tr data-board='${esc(b)}'>${name}<td>${m.status(S.FAILURE).board_icon}</td><td colspan='3'>Core build failed ${link(l.job_link, 'log')}</td></tr>`;
  }
  const warn = l ? `${m.status(l.status).board_icon} ${l.warnings?.length ? `${m.p.icons.warnings} ${l.warnings.length}` : m.p.icons.no_warnings}` : '–';
  const worst = tests.length ? m.status(Math.max(...tests.map((t) => t.status))).board_icon : '';
  const ram = l?.meminfo?.RAM;
  const ramCell = ram ? `${bar(ram[0], ram[1])} ${pct(ram[0], ram[1])}` : '–';
  return `<tr data-board='${esc(b)}'>${name}<td>${warn}</td><td>${worst ? `${worst} ` : ''}${results(m, tests)}</td>`
    + `<td class='n'>${ramCell}</td><td>${logs.join(' · ')}</td></tr>`;
}

// whether the boards of an artifact are shown in the recap (native_sim
// collapsed by default, but on its own page); with a board selection in a
// single artifact, there are no artifact headers and all are shown
function recapOpen(m, view, a) {
  const kind = (view.boards ?? { kind: 'all' }).kind;
  if (kind !== 'all' && kind !== 'soc') return true;
  const def = a !== 'native_sim' || m.nativeOnly;
  return view.collapsed?.has(`recap:${a}`) ? !def : def;
}

function renderRecap(m, view) {
  // the tests of the selections and of the text filter, as for the counts
  // of the filter bar; with a test selection or a filter, only the boards
  // with such tests (or, with no test selection, a matching loader)
  const tests = filteredTests(m, view);
  const allTests = (view.tests ?? { kind: 'all' }).kind === 'all';
  const shown = (b) => (allTests && !view.filter) || tests.some((t) => t.board === b)
    || (allTests && matches(loaderText(m, b), view.filter));
  const sel = view.boards ?? { kind: 'all' };
  const inSel = new Set(selectionBoards(m, sel) ?? []);
  // artifact headers when the selection can span artifacts
  const headers = sel.kind === 'all' || sel.kind === 'soc';
  const rows = [];
  for (const a of m.artifacts) {
    const boards = (m.boardsByArtifact.get(a) ?? []).filter((b) => (sel.kind === 'all' || inSel.has(b)) && shown(b));
    if (!boards.length) continue;
    if (headers) {
      // collapsible artifact header, with the results of its boards
      const key = `recap:${a}`;
      const open = recapOpen(m, view, a);
      const at = tests.filter((t) => boards.includes(t.board));
      rows.push(`<tr class='art' data-toggle='${esc(key)}'><td colspan='5'><span class='arrow'>${open ? '▾' : '▸'}</span> ${esc(a)}`
        + ` <span class='count'>(${boards.length} boards)</span> ${results(m, at)}</td></tr>`);
      if (!open) continue;
    }
    for (const b of boards) rows.push(recapRow(m, view, b, tests.filter((t) => t.board === b)));
  }
  return table(['Board', 'Loader', 'Tests', 'RAM', 'Logs'], rows, 'recap');
}

// --- tabs

const TABS = [['issues', 'Test issues'], ['warnings', 'Loader warnings'], ['memory', 'Memory'],
  ['kconfig', 'Kconfig'], ['sizes', 'Sketch sizes']];

function renderTabs(m, view) {
  const tab = view.tab ?? 'issues';
  const tests = filteredTests(m, view);
  const issues = tests.filter((t) => t.status >= m.S.INVALID_EXCEPTION).length;
  const warnBoards = selectedBoards(m, view).filter((b) => m.p.loaders[b]?.warnings?.length).length;
  // (the issue count only when there are some)
  const label = (id, text) => (id === 'issues' ? (issues ? `${text} ${m.status(m.S.ERROR).test_icon} ${issues}` : text)
    : id === 'warnings' ? `${text} (${warnBoards})` : text);
  const buttons = TABS.map(([id, text]) => `<button${id === tab ? " class='on'" : ''} data-tab='${id}'>${label(id, text)}</button>`).join('');
  return `<nav id='tabs'>${buttons}</nav><div id='tab'>${renderTab(m, view, tab)}</div>`;
}

function renderTab(m, view, tab) {
  const fn = { issues: renderIssuesTab, warnings: renderWarningsTab, memory: renderMemoryTab,
               kconfig: renderKconfigTab, sizes: renderSizesTab }[tab] ?? renderIssuesTab;
  return fn(m, view);
}

// --- Test issues tab

// short stable id of a text, for the keys of the sections
function hashKey(text) {
  let h = 5381;
  for (let i = 0; i < text.length; i++) h = ((h * 33) ^ text.charCodeAt(i)) >>> 0;
  return h.toString(36);
}

function card(m, view, c, group) {
  const S = m.S;
  const labels = c.sketches.map((s) => {
    const t = m.p.tests.find((x) => sketchKey(x.sketch) === s);
    return t ? m.sketchLabel(t) : s;
  });
  // one chip per (board, link mode), with its worst status
  const chips = new Map();
  for (const h of c.hits) {
    const k = `${h.board}\n${h.link_mode}`;
    if (!chips.has(k) || chips.get(k).status < h.status) chips.set(k, h);
  }
  const shown = new Set(hideTwins(m, view, [...new Set(c.hits.map((h) => h.board))]));
  const chipHtml = [...chips.values()].filter((h) => shown.has(h.board)).map((h) => `<a class='chip st-${m.status(h.status).name.toLowerCase()}' href='${esc(h.job_link)}'`
    + ` title='${esc(m.status(h.status).name)}'>${hl(m.label(h.board), view.filter)}${h.link_mode ? ` ${esc(h.link_mode)}` : ''}`
    + `${ranIcon(m, h.board, c.hits.filter((x) => x.board === h.board).map((x) => x.sketch))} ↗</a>`).join('');
  const more = c.sketches.length > 1 ? ` <span class='count'>— ${c.sketches.length} sketches${c.text ? ', same error' : ''}</span>` : '';
  const rawKey = `raw:${group}:${hashKey(c.key)}`;
  // the full output of the jobs is rendered only when opened (page weight)
  const rawOpen = isOpen(view, rawKey, false);
  const raw = c.text ? `<details${rawOpen ? ' open' : ''} class='raw' data-key='${rawKey}'><summary>full output</summary>${rawOpen ? c.hits.map((h) =>
    `<div class='rawhit'>${code(`${m.label(h.board)} ${h.link_mode} ${h.sketch}`)}<pre>${hl(h.raw, view.filter)}</pre></div>`).join('') : ''}</details>` : '';
  return `<div class='card st-${m.status(c.status).name.toLowerCase()}'><h4>${m.status(c.status).test_icon} `
    + `${labels.map((l) => hl(l, view.filter)).join(', ')}${more}</h4><div class='chips'>${chipHtml}</div>`
    + `${c.text ? `<pre>${hl(c.text, view.filter)}</pre>` : ''}${raw}</div>`;
}

function renderIssuesTab(m, view) {
  const S = m.S;
  const selected = selectTests(m, view);
  if (!selected.length) return note('no tests for this selection');
  const f = view.filter ?? '';
  // the tests matching the filter, as for the counts of the filter bar and
  // of the sidebar
  const g = cardGroups(m, selected.filter((t) => matches(testText(m, t), f)));
  const anyIssues = selected.some((t) => t.status >= S.INVALID_EXCEPTION);
  // sections rendered with their content only when open (page weight); the
  // filter does not open them: their heading counts the matches
  const sec = (key, status, title, items, count, body, def) => {
    if (!items.length) return '';
    const open = isOpen(view, key, def);
    return `<details${open ? ' open' : ''} data-key='${key}'><summary>${m.status(status).test_icon} ${title} <span class='count'>(${count})</span></summary>${open ? body() : ''}</details>`;
  };
  const cards = (list, group) => list.map((c) => card(m, view, c, group)).join('');
  const nHits = (list) => list.reduce((n, c) => n + c.hits.length, 0);
  const { issues, known, warnings, passed, skipped } = g;
  const passedBody = () => `<ul class='list'>${passed.map((r) => `<li>${code(r.label)} — ${hideTwins(m, view, [...new Set(r.boards.map((b) => b.board))])
    .map((b) => `${hl(m.label(b), f)}${ranIcon(m, b, [r.sketch])}`).join(', ')}</li>`).join('')}</ul>`;
  // (as the passed list, one entry per sketch with its boards, under the
  // reason they were skipped for)
  const skippedBody = () => [...new Set(skipped.map((r) => r.reason))].map((reason) => `<h5 class='list-title'>${hl(reason, f)}</h5><ul class='list'>`
    + `${skipped.filter((r) => r.reason === reason).map((r) => `<li>${code(r.label)} — ${hideTwins(m, view, r.boards)
      .map((b) => hl(m.label(b), f)).join(', ')}</li>`).join('')}</ul>`).join('');
  return (issues.length ? '' : note(f && anyIssues ? 'no issues match the filter' : 'no issues'))
    + sec('issues', S.ERROR, 'Issues', issues, nHits(issues), () => cards(issues, 'issues'), true)
    + sec('known', S.EXPECTED_ERROR, 'Known failures', known, nHits(known), () => cards(known, 'known'), false)
    + sec('warnings', S.WARNING, 'Warnings', warnings, nHits(warnings), () => cards(warnings, 'warnings'), false)
    + sec('passed', S.PASS, 'Passed', passed, passed.reduce((n, r) => n + r.boards.length, 0), passedBody, false)
    + sec('skipped', S.SKIP, 'Skipped', skipped, skipped.reduce((n, r) => n + r.boards.length, 0), skippedBody, false);
}

// --- board tabs (boards of the selections) and sketch sizes

const boardCell = (m, b) => `<td><button class='blink' data-sel='boards' data-kind='board' data-id='${esc(b)}'>${code(m.label(b))}</button></td>`;
const EMPTY = note('nothing to show for this selection');

function renderWarningsTab(m, view) {
  const f = view.filter ?? '';
  const cards = selectedBoards(m, view).filter((b) => m.p.loaders[b]?.warnings?.length)
    .filter((b) => matches([m.label(b), ...m.p.loaders[b].warnings].join('\n'), f))
    .map((b) => {
      const l = m.p.loaders[b];
      return `<div class='card'><h4><button class='blink' data-sel='boards' data-kind='board' data-id='${esc(b)}'>${code(m.label(b))}</button>`
        + ` ${link(l.job_link, 'loader log ↗')}</h4><pre>${hl(l.warnings.join('\n'), f)}</pre></div>`;
    });
  return cards.join('') || EMPTY;
}

// boards and memory regions of the memory tab: the regions shared by all
// the boards, or all of them with the checkbox (view.memAll)
function memoryTable(m, view) {
  const f = view.filter ?? '';
  const boards = selectedBoards(m, view).filter((b) => Object.keys(m.p.loaders[b]?.meminfo ?? {}).length)
    .filter((b) => matches(`${m.label(b)}\n${b}`, f));
  const sets = boards.map((b) => Object.keys(m.p.loaders[b].meminfo));
  const regions = (view.memAll ? [...new Set(sets.flat())] : (sets[0] ?? []).filter((r) => sets.every((x) => x.includes(r)))).sort();
  return { boards, regions };
}

function renderMemoryTab(m, view) {
  const { boards, regions } = memoryTable(m, view);
  const rows = boards.map((b) => `<tr>${boardCell(m, b)}${regions.map((r) => {
    const v = m.p.loaders[b].meminfo[r];
    return v ? `<td class='n'>${esc(v[0])} / ${esc(v[1])} ${bar(v[0], v[1])} <sub>${pct(v[0], v[1])}</sub></td>` : "<td class='n'>–</td>";
  }).join('')}</tr>`);
  const tools = `<div class='tools'><label><input type='checkbox' data-memall${view.memAll ? ' checked' : ''}> all regions</label>`
    + (rows.length ? ` <button data-copy='memory'>copy as CSV</button>` : '') + '</div>';
  return tools + (table(['Board', ...regions.map(esc)], rows) || EMPTY);
}

// CSV of the memory table: used, size and percent of each region
function memoryCsv(m, view) {
  const { boards, regions } = memoryTable(m, view);
  const cell = (v) => (/[",\n]/.test(String(v)) ? `"${String(v).replace(/"/g, '""')}"` : String(v));
  const head = ['board', ...regions.flatMap((r) => [`${r} used`, `${r} size`, `${r} %`])];
  const rows = boards.map((b) => [m.label(b), ...regions.flatMap((r) => {
    const v = m.p.loaders[b].meminfo[r];
    return v && isNum(v[0]) && isNum(v[1]) ? [v[0], v[1], v[1] > 0 ? (100 * v[0] / v[1]).toFixed(1) : ''] : ['', '', ''];
  })]);
  return [head, ...rows].map((r) => r.map(cell).join(',')).join('\n');
}

// Kconfig symbols shown for several boards (view.kconfigSet): the ones with
// differing values (highlighted in the table, the default), the ones
// defined in all the boards, or all of them; a search shows all its matches
const KCONFIG_SETS = [['diff', 'differing values'], ['shared', 'defined in all boards'], ['all', 'all symbols']];

function renderKconfigTab(m, view) {
  const f = view.filter ?? '';
  // (not native_sim, a test only board, and the boards it poses as)
  const boards = selectedBoards(m, view).filter((b) => m.p.loaders[b]?.config && m.p.loaders[b].artifact !== 'native_sim');
  const names = [...new Set(boards.flatMap((b) => Object.keys(m.p.loaders[b].config)))].sort();
  const textOf = (name, values) => values.flatMap((v) => (v === undefined ? [] : kconfigTexts(name, v))).join('\n') || name;
  if (boards.length === 1) {
    const cfg = m.p.loaders[boards[0]].config;
    const items = names.filter((n) => matches(textOf(n, [cfg[n]]), f))
      .map((n) => `<li>${code(n)} = ${hl(cfg[n], f.includes('=') ? '' : f)}</li>`);
    return items.length ? `<ul class='kv'>${items.join('')}</ul>` : EMPTY;
  }
  const set = f ? 'all' : view.kconfigSet ?? 'diff';
  const rows = [];
  for (const name of names) {
    const values = boards.map((b) => m.p.loaders[b].config[name]);
    if (!matches(textOf(name, values), f)) continue;
    // differing values among the boards that define it (most options are
    // missing on some board: that alone would mark nearly every row)
    const diff = new Set(values.filter((v) => v !== undefined).map((v) => JSON.stringify(v))).size > 1;
    if ((set === 'diff' && !diff) || (set === 'shared' && values.includes(undefined))) continue;
    rows.push(`<tr${diff ? " class='diff'" : ''}><td>${code(name)}</td>${values.map((v) => `<td>${v === undefined ? '' : esc(v)}</td>`).join('')}</tr>`);
  }
  const cur = view.kconfigSet ?? 'diff';
  const tools = boards.length < 2 ? '' : `<div class='tools'>${KCONFIG_SETS.map(([id, text]) =>
    `<button${id === cur ? " class='on'" : ''} data-kcset='${id}'${f ? ' disabled' : ''}>${text}</button>`).join('')}</div>`;
  return tools + (table(['Option', ...boards.map((b) => code(m.label(b)))], rows, 'kconfig') || EMPTY);
}

function renderSizesTab(m, view) {
  const f = view.filter ?? '';
  const deltas = m.p.size_deltas;
  const boards = new Set(selectedBoards(m, view));
  const sel = view.tests ?? { kind: 'all' };
  const sketches = sel.kind === 'group' ? new Set(m.groups.get(sel.id) ?? []) : sel.kind === 'sketch' ? new Set([sel.id]) : null;
  const labelOf = (sketch) => { const t = m.p.tests.find((x) => sketchKey(x.sketch) === sketchKey(sketch)); return t ? m.sketchLabel(t) : sketch; };
  const num = (v) => (isNum(v) ? String(v) : '–');
  const delta = (d) => (deltas !== 'available' ? '' : isNum(d) && d !== 0 ? `<td class='n ${d > 0 ? 'up' : 'dn'}'>${signed(d)}</td>` : `<td class='n'>${isNum(d) ? 0 : ''}</td>`);
  const fmt = (v) => `<td class='n'>${num(v?.used)}</td><td class='n'>${pct(v?.used, v?.max)}</td>${delta(v?.delta)}`;
  const rows = m.p.sizes.filter((s) => boards.has(s.board) && (!sketches || sketches.has(sketchKey(s.sketch))))
    .filter((s) => matches([s.sketch, labelOf(s.sketch), m.label(s.board), s.board, s.link_mode].join('\n'), f))
    .sort((a, b) => labelOf(a.sketch).localeCompare(labelOf(b.sketch)) || m.label(a.board).localeCompare(m.label(b.board))
      || a.link_mode.localeCompare(b.link_mode))
    .map((s) => `<tr><td title='${esc(s.sketch)}'>${code(labelOf(s.sketch))}</td>${boardCell(m, s.board)}<td>${esc(s.link_mode)}</td>${fmt(s.flash)}${fmt(s.ram)}</tr>`);
  if (!rows.length) return EMPTY;
  const d = deltas === 'available' ? ['Δ'] : [];
  return (deltas === 'unavailable' ? note('no delta (base reports unavailable)') : '')
    + table(['Sketch', 'Board', 'Link mode', 'Flash', '%', ...d, 'RAM', '%', ...d], rows);
}

// --- page

function renderPage(m, view) {
  return `<aside id='sidebar'>${renderSidebar(m, view)}</aside><main id='main'>${renderMain(m, view)}</main>`;
}

// light/dark toggle, at the top right of the header, labelled with the
// theme it switches to
const themeToggle = (view) => `<button class='theme' data-theme-toggle title='switch theme'>${view.theme === 'dark' ? '☀ light' : '☾ dark'}</button>`;

const renderMain = (m, view) => renderFilterBar(m, view) + `<div class='pagehead'>${themeToggle(view)}${renderHeader(m)}</div>`
  + renderRecap(m, view) + renderTabs(m, view);

// whether two elements are the same control (data attributes), to focus it
// again after a render
const CONTROL_KEYS = ['sel', 'kind', 'id', 'tab', 'toggle', 'clear', 'memall', 'kcset', 'themeToggle', 'copy'];
const sameControl = (a, b) => CONTROL_KEYS.some((k) => a[k] !== undefined) && CONTROL_KEYS.every((k) => a[k] === b[k]);

// view presets from the URL query (direct links to the page: GitHub drops
// the query on the artifact link); unknown ids are ignored
// selection of a sidebar entry: the board (artifact, board, SoC) and the
// test selections are exclusive; clicking the selected entry, or All,
// goes back to all
function selectEntry(view, side, kind, id) {
  const cur = view[side] ?? { kind: 'all' };
  const sel = kind === 'all' || (cur.kind === kind && cur.id === id) ? { kind: 'all' } : { kind, id };
  return { boards: { kind: 'all' }, tests: { kind: 'all' }, [side]: sel };
}

function readPresets(search, m) {
  const q = new URLSearchParams(search);
  const out = {};
  const boards = new Set(m.artifacts.flatMap((a) => m.boardsByArtifact.get(a)));
  const sketches = new Set([...m.groups.values()].flat());
  if (q.get('filter')) out.filter = q.get('filter');
  if (m.artifacts.includes(q.get('artifact'))) out.boards = { kind: 'artifact', id: q.get('artifact') };
  if (m.socs.has(q.get('soc'))) out.boards = { kind: 'soc', id: q.get('soc') };
  if (boards.has(q.get('board'))) out.boards = { kind: 'board', id: q.get('board') };
  if (q.has('group') && m.groups.has(q.get('group'))) out.tests = { kind: 'group', id: q.get('group') };
  if (sketches.has(q.get('sketch'))) out.tests = { kind: 'sketch', id: q.get('sketch') };
  // one selection only (see selectEntry): the test one wins
  if (out.tests) delete out.boards;
  return out;
}

const api = {
  escapeHtml: esc, matches, hl, buildModel, selectTests, badge, issueKey, cardGroups,
  renderSidebar, renderFilterBar, renderHeader, pageTitle, renderRecap, renderTabs, renderIssuesTab,
  renderWarningsTab, renderMemoryTab, memoryCsv, renderKconfigTab, renderSizesTab, renderPage,
  readPresets, selectEntry, sameControl,
};
if (typeof module !== 'undefined' && module.exports) module.exports = api;
root.CIReport = api;

// --- DOM layer (browser only)

async function decode(text) {
  const bytes = Uint8Array.from(atob(text.trim()), (c) => c.charCodeAt(0));
  const stream = new Blob([bytes]).stream().pipeThrough(new DecompressionStream('gzip'));
  return JSON.parse(await new Response(stream).text());
}

function start(p) {
  const m = buildModel(p);
  const view = { filter: '', boards: { kind: 'all' }, tests: { kind: 'all' }, tab: 'issues',
                 open: new Set(), collapsed: new Set(), ...readPresets(location.search, m),
                 theme: document.documentElement.dataset.theme === 'dark' ? 'dark' : 'light' };
  const app = document.getElementById('app');
  document.title = pageTitle(m);
  app.innerHTML = "<aside id='sidebar'></aside><main id='main'></main>";
  const sidebar = document.getElementById('sidebar');
  const main = document.getElementById('main');

  function render() {
    // keep the focus and the caret of the filter box across renders
    const input = document.activeElement?.id === 'filter' ? document.activeElement : null;
    const caret = input ? [input.selectionStart, input.selectionEnd] : null;
    // and of the control that was clicked (keyboard navigation)
    const active = document.activeElement;
    const control = active && active !== input && app.contains(active) ? { ...active.dataset } : null;
    const scroll = sidebar.scrollTop;
    sidebar.innerHTML = renderSidebar(m, view);
    sidebar.scrollTop = scroll;
    main.innerHTML = renderMain(m, view);
    if (control) {
      for (const b of app.querySelectorAll('button')) {
        if (sameControl(b.dataset, control)) { b.focus(); break; }
      }
    }
    if (caret) {
      const f = document.getElementById('filter');
      f.focus();
      f.setSelectionRange(...caret);
    }
    fitRecap();
    pinHeaders();
  }

  // shrink the room on the right of the recap columns (50px down to 8px)
  // when the table does not fit its box on one line per row (the browser
  // would rather wrap the cells than shrink the padding)
  function fitRecap() {
    const table = main.querySelector('table.recap');
    if (!table) return;
    table.style.removeProperty('--recap-pad');
    table.style.width = 'max-content';
    const over = table.offsetWidth - table.parentElement.clientWidth;
    table.style.width = '';
    if (over > 0) {
      const cols = table.rows[0]?.cells.length || 1;
      table.style.setProperty('--recap-pad', `${Math.max(8, 50 - Math.ceil(over / cols))}px`);
    }
  }

  // keep the header row of the tab tables visible below the pinned bars: a
  // sticky header would stick in the box that scrolls the wide tables
  // sideways, not in the page
  function pinHeaders() {
    const pin = document.getElementById('tabs')?.getBoundingClientRect().bottom ?? 0;
    for (const head of main.querySelectorAll('#tab thead')) {
      const table = head.closest('table').getBoundingClientRect();
      const shift = Math.max(0, Math.min(pin - table.top, table.height - head.offsetHeight));
      for (const th of head.querySelectorAll('th')) th.style.transform = shift ? `translateY(${shift}px)` : '';
    }
  }
  window.addEventListener('scroll', pinHeaders, { passive: true });
  window.addEventListener('resize', () => { fitRecap(); pinHeaders(); });

  const select = (side, kind, id) => Object.assign(view, selectEntry(view, side, kind, id));

  let timer; // debounce of the filter box
  app.addEventListener('click', (e) => {
    const el = e.target.closest('[data-sel], [data-clear], [data-toggle], [data-tab], [data-memall], [data-kcset], [data-theme-toggle], [data-copy], summary, tr[data-board]');
    if (!el) return;
    if (el.dataset.copy === 'memory') {
      // no render: only the button text changes
      const done = (text) => { el.textContent = text; setTimeout(() => { el.textContent = 'copy as CSV'; }, 1500); };
      navigator.clipboard.writeText(memoryCsv(m, view)).then(() => done('copied ✓'), () => done('copy failed'));
      return;
    }
    if (el.matches('summary')) {
      // remember the sections opened or closed by the user
      const details = el.parentElement;
      const key = details?.dataset?.key;
      if (!key) return;
      view.open.delete(key);
      view.open.delete(`!${key}`);
      view.open.add(details.open ? `!${key}` : key);
      // closed sections have no content: render it (also toggles the section)
      e.preventDefault();
    } else if (el.dataset.memall !== undefined) {
      view.memAll = !view.memAll;
    } else if (el.dataset.kcset) {
      view.kconfigSet = el.dataset.kcset;
    } else if (el.dataset.themeToggle !== undefined) {
      // saved only when chosen: until then, the page follows the system
      view.theme = view.theme === 'dark' ? 'light' : 'dark';
      document.documentElement.dataset.theme = view.theme;
      try { localStorage.setItem('arduino-ci-report:theme', view.theme); } catch (err) { /* not saved */ }
    } else if (el.dataset.toggle !== undefined) {
      const key = el.dataset.toggle;
      if (view.collapsed.has(key)) view.collapsed.delete(key); else view.collapsed.add(key);
    } else if (el.dataset.sel) {
      select(el.dataset.sel, el.dataset.kind, el.dataset.id);
    } else if (el.dataset.clear) {
      const c = el.dataset.clear;
      if (c === 'boards' || c === 'all') view.boards = { kind: 'all' };
      if (c === 'tests' || c === 'all') view.tests = { kind: 'all' };
      if (c === 'filter' || c === 'all') { clearTimeout(timer); view.filter = ''; view.filterRaw = ''; }
    } else if (el.dataset.tab) {
      view.tab = el.dataset.tab;
    } else if (el.matches('tr[data-board]')) {
      if (e.target.closest('a')) return; // log links
      select('boards', 'board', el.dataset.board);
    }
    render();
  });

  app.addEventListener('input', (e) => {
    if (e.target.id !== 'filter') return;
    clearTimeout(timer);
    timer = setTimeout(() => { view.filterRaw = e.target.value; view.filter = e.target.value.trim(); render(); }, 150);
  });

  render();
}

if (typeof document !== 'undefined') {
  const data = document.getElementById('report-data');
  if (data) {
    decode(data.textContent).then(start).catch((e) => {
      document.getElementById('app').innerHTML = `<p>Cannot load the report data: ${esc(e)}</p>`;
    });
  }
}

})(typeof window !== 'undefined' ? window : globalThis);
