// Lightweight confirmation modal — returns Promise<boolean>
export function showConfirm(message) {
    return new Promise(resolve => {
        const overlay = document.createElement('div');
        overlay.className = 'confirm-overlay';
        overlay.innerHTML = `
            <div class="confirm-dialog">
                <p class="confirm-message">${escHtml(message)}</p>
                <div class="confirm-actions">
                    <button class="btn-sm confirm-cancel">Cancel</button>
                    <button class="btn-sm confirm-ok">OK</button>
                </div>
            </div>
        `;

        const cancel = overlay.querySelector('.confirm-cancel');
        const ok = overlay.querySelector('.confirm-ok');

        function close(result) {
            overlay.remove();
            resolve(result);
        }

        cancel.onclick = () => close(false);
        ok.onclick = () => close(true);
        overlay.addEventListener('click', e => { if (e.target === overlay) close(false); });

        document.body.appendChild(overlay);
        ok.focus();
    });
}

function escHtml(s) {
    return (s || '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}
