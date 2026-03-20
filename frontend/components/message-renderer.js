export function renderMessage(msg) {
    const el = document.createElement('div');
    el.className = `message ${msg.role}`;

    if (msg.role === 'user') {
        el.textContent = msg.content || msg.text || '';
        return el;
    }

    if (msg.role === 'system') {
        el.textContent = msg.content || msg.text || '';
        return el;
    }

    // Assistant message — render markdown
    const html = renderMarkdown(msg.content || msg.text || '');
    el.innerHTML = html;

    // Add copy buttons to code blocks
    el.querySelectorAll('.code-block').forEach(block => {
        const copyBtn = block.querySelector('.code-block-copy');
        if (copyBtn) {
            copyBtn.onclick = () => {
                const code = block.querySelector('code')?.textContent || '';
                navigator.clipboard?.writeText(code).then(() => {
                    copyBtn.textContent = 'Copied!';
                    setTimeout(() => copyBtn.textContent = 'Copy', 1500);
                });
            };
        }
    });

    return el;
}

export function renderToolCall(tc) {
    const el = document.createElement('div');
    el.className = 'tool-call';

    const header = document.createElement('div');
    header.className = 'tool-call-header';
    header.innerHTML = `<span class="arrow">&#9654;</span> Using tool: <strong>${esc(tc.name)}</strong>`;

    const body = document.createElement('div');
    body.className = 'tool-call-body';
    body.textContent = JSON.stringify(tc.arguments, null, 2);

    header.onclick = () => {
        header.classList.toggle('expanded');
        body.classList.toggle('visible');
    };

    el.appendChild(header);
    el.appendChild(body);
    return el;
}

export function renderToolResult(tr) {
    const el = document.createElement('div');
    el.className = 'tool-call';

    const header = document.createElement('div');
    header.className = 'tool-call-header';
    header.innerHTML = `<span class="arrow">&#9654;</span> Result: <strong>${esc(tr.name || '')}</strong>`;

    const body = document.createElement('div');
    body.className = 'tool-call-body';
    body.textContent = typeof tr.content === 'string' ? tr.content : JSON.stringify(tr.content, null, 2);

    header.onclick = () => {
        header.classList.toggle('expanded');
        body.classList.toggle('visible');
    };

    el.appendChild(header);
    el.appendChild(body);
    return el;
}

function renderMarkdown(text) {
    if (typeof marked !== 'undefined') {
        marked.setOptions({
            highlight: (code, lang) => {
                if (typeof hljs !== 'undefined' && lang && hljs.getLanguage(lang)) {
                    return hljs.highlight(code, { language: lang }).value;
                }
                return esc(code);
            },
            breaks: true
        });

        // Custom renderer for code blocks with copy button
        const renderer = new marked.Renderer();
        renderer.code = (code, lang) => {
            let highlighted = esc(code);
            if (typeof hljs !== 'undefined' && lang && hljs.getLanguage(lang)) {
                highlighted = hljs.highlight(code, { language: lang }).value;
            }
            return `<div class="code-block"><div class="code-block-header"><span>${esc(lang || 'code')}</span><button class="code-block-copy">Copy</button></div><pre><code>${highlighted}</code></pre></div>`;
        };

        return marked.parse(text, { renderer });
    }
    return `<p>${esc(text)}</p>`;
}

function esc(s) {
    return (s || '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}
