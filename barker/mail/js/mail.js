document.addEventListener('DOMContentLoaded', function () {

    // ===== Menu mobile (sidebar en tiroir) =====
    var toggle = document.getElementById('mlMenuToggle');
    var sidebar = document.getElementById('mlSidebar');
    var overlay = document.getElementById('mlSidebarOverlay');

    function closeSidebar() {
        if (sidebar) sidebar.classList.remove('open');
        if (overlay) overlay.classList.remove('visible');
    }

    if (toggle && sidebar) {
        toggle.addEventListener('click', function () {
            var open = sidebar.classList.toggle('open');
            if (overlay) overlay.classList.toggle('visible', open);
        });
    }
    if (overlay) overlay.addEventListener('click', closeSidebar);
    if (sidebar) {
        sidebar.querySelectorAll('a').forEach(function (a) {
            a.addEventListener('click', closeSidebar);
        });
    }

    // ===== Aperçu instantané du thème sur la page Paramètres =====
    var themeRadios = document.querySelectorAll('.ml-theme-choice input[name="theme"]');
    themeRadios.forEach(function (radio) {
        radio.addEventListener('change', function () {
            document.documentElement.setAttribute('data-theme', radio.value);
            themeRadios.forEach(function (r) {
                var card = r.closest('.ml-radio-card');
                if (card) card.classList.toggle('checked', r.checked);
            });
        });
    });

    // ===== "Tout sélectionner" avec état indéterminé =====
    var selectAll = document.getElementById('selectAll');
    var rowchecks = document.querySelectorAll('.rowcheck');
    function refreshSelectAll() {
        if (!selectAll || !rowchecks.length) return;
        var checkedCount = 0;
        rowchecks.forEach(function (c) { if (c.checked) checkedCount++; });
        selectAll.checked = checkedCount === rowchecks.length;
        selectAll.indeterminate = checkedCount > 0 && checkedCount < rowchecks.length;
    }
    rowchecks.forEach(function (c) { c.addEventListener('change', refreshSelectAll); });
    refreshSelectAll();

    // ===== Aperçu des pièces jointes sélectionnées (compose) =====
    var fileInput = document.getElementById('attachments');
    var fileList = document.getElementById('attachmentsList');
    if (fileInput && fileList) {
        fileInput.addEventListener('change', function () {
            fileList.innerHTML = '';
            Array.prototype.forEach.call(fileInput.files, function (f) {
                var chip = document.createElement('span');
                chip.className = 'ml-file-chip';
                var sizeKb = Math.round(f.size / 102.4) / 10;
                chip.innerHTML = '<i class="fa-solid fa-paperclip"></i> ' + f.name + ' <small>(' + sizeKb + ' Ko)</small>';
                fileList.appendChild(chip);
            });
        });
    }
});
