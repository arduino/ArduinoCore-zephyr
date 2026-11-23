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

// sketch identity: the path in the core ('libraries/...'), the tests name
// the sketches from its parent folder ('ArduinoCore-zephyr/libraries/...')
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

  const artifacts = [...boardsByArtifact.keys()];

  // test groups (library or examples group, '' for the CI entries) and their sketches
  const groups = new Map();
  for (const t of p.tests) {
    if (!groups.has(t.group)) groups.set(t.group, new Set());
    groups.get(t.group).add(sketchKey(t.sketch));
  }
  for (const [g, set] of groups) groups.set(g, [...set].sort());

  return {
    p, statuses, S: byName,
    artifacts,
    boardsByArtifact,
    groups: new Map([...groups].sort(([a], [b]) => a.localeCompare(b))),
    status: (value) => statuses.get(value) ?? statuses.get(byName.SKIP),
    label: (board) => board,
    sketchLabel: (t) => (t.group ? `${t.group} ${t.name}` : t.name),
  };
}

// tests of the board selection (all, artifact, board) and of the test
// selection (all, group, sketch); either side can be ignored
function selectTests(m, view, { boards = true, tests = true } = {}) {
  const b = view.boards ?? { kind: 'all' };
  const s = view.tests ?? { kind: 'all' };
  return m.p.tests.filter((t) =>
    (!boards || b.kind === 'all' || (b.kind === 'artifact' ? t.artifact === b.id : t.board === b.id))
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

const api = {
  escapeHtml: esc, buildModel, selectTests, badge, issueKey, cardGroups,
};
if (typeof module !== 'undefined' && module.exports) module.exports = api;
root.CIReport = api;

})(typeof window !== 'undefined' ? window : globalThis);
