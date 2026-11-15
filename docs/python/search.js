'use strict';
(function () {
  const input = document.getElementById('api-search');
  const results = document.getElementById('search-results');
  const status = document.getElementById('search-status');
  const entries = window.AMS_MEL_PYTHON_INDEX;
  if (!input || !results || !status || !Array.isArray(entries)) return;
  function search() {
    const query = input.value.trim().toLocaleLowerCase();
    results.replaceChildren();
    results.hidden = !query;
    if (!query) {
      status.textContent = 'The complete alphabetical directory is available below.';
      return;
    }
    const words = query.split(/\s+/);
    const matches = entries.filter(function (entry) {
      const text = (entry.name + ' ' + entry.kind + ' ' + entry.description).toLocaleLowerCase();
      return words.every(function (word) { return text.includes(word); });
    });
    matches.forEach(function (entry) {
      const item = document.createElement('li');
      const link = document.createElement('a');
      link.href = entry.url;
      link.textContent = entry.name;
      item.append(link, document.createTextNode(' — ' + entry.kind +
        (entry.description ? ': ' + entry.description : '')));
      results.append(item);
    });
    status.textContent = matches.length + (matches.length === 1 ? ' matching API item.' : ' matching API items.');
  }
  input.addEventListener('input', search);
}());
