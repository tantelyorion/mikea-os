const COUNTRIES = [
{code:"MG",name:"Madagascar",dial:"261"},
{code:"FR",name:"France",dial:"33"},
{code:"US",name:"États-Unis",dial:"1"},
{code:"CA",name:"Canada",dial:"1"},
{code:"GB",name:"Royaume-Uni",dial:"44"},
{code:"DE",name:"Allemagne",dial:"49"},
{code:"IT",name:"Italie",dial:"39"},
{code:"ES",name:"Espagne",dial:"34"},
{code:"PT",name:"Portugal",dial:"351"},
{code:"BE",name:"Belgique",dial:"32"},
{code:"CH",name:"Suisse",dial:"41"},
{code:"NL",name:"Pays-Bas",dial:"31"},
{code:"LU",name:"Luxembourg",dial:"352"},
{code:"IE",name:"Irlande",dial:"353"},
{code:"AT",name:"Autriche",dial:"43"},
{code:"SE",name:"Suède",dial:"46"},
{code:"NO",name:"Norvège",dial:"47"},
{code:"DK",name:"Danemark",dial:"45"},
{code:"FI",name:"Finlande",dial:"358"},
{code:"IS",name:"Islande",dial:"354"},
{code:"PL",name:"Pologne",dial:"48"},
{code:"CZ",name:"Tchéquie",dial:"420"},
{code:"SK",name:"Slovaquie",dial:"421"},
{code:"HU",name:"Hongrie",dial:"36"},
{code:"RO",name:"Roumanie",dial:"40"},
{code:"BG",name:"Bulgarie",dial:"359"},
{code:"GR",name:"Grèce",dial:"30"},
{code:"HR",name:"Croatie",dial:"385"},
{code:"SI",name:"Slovénie",dial:"386"},
{code:"RS",name:"Serbie",dial:"381"},
{code:"BA",name:"Bosnie-Herzégovine",dial:"387"},
{code:"ME",name:"Monténégro",dial:"382"},
{code:"MK",name:"Macédoine du Nord",dial:"389"},
{code:"AL",name:"Albanie",dial:"355"},
{code:"XK",name:"Kosovo",dial:"383"},
{code:"MD",name:"Moldavie",dial:"373"},
{code:"UA",name:"Ukraine",dial:"380"},
{code:"BY",name:"Biélorussie",dial:"375"},
{code:"RU",name:"Russie",dial:"7"},
{code:"LT",name:"Lituanie",dial:"370"},
{code:"LV",name:"Lettonie",dial:"371"},
{code:"EE",name:"Estonie",dial:"372"},
{code:"MT",name:"Malte",dial:"356"},
{code:"CY",name:"Chypre",dial:"357"},
{code:"AD",name:"Andorre",dial:"376"},
{code:"MC",name:"Monaco",dial:"377"},
{code:"SM",name:"Saint-Marin",dial:"378"},
{code:"VA",name:"Vatican",dial:"379"},
{code:"LI",name:"Liechtenstein",dial:"423"},
{code:"MA",name:"Maroc",dial:"212"},
{code:"DZ",name:"Algérie",dial:"213"},
{code:"TN",name:"Tunisie",dial:"216"},
{code:"LY",name:"Libye",dial:"218"},
{code:"EG",name:"Égypte",dial:"20"},
{code:"SD",name:"Soudan",dial:"249"},
{code:"SS",name:"Soudan du Sud",dial:"211"},
{code:"ET",name:"Éthiopie",dial:"251"},
{code:"ER",name:"Érythrée",dial:"291"},
{code:"DJ",name:"Djibouti",dial:"253"},
{code:"SO",name:"Somalie",dial:"252"},
{code:"KE",name:"Kenya",dial:"254"},
{code:"UG",name:"Ouganda",dial:"256"},
{code:"TZ",name:"Tanzanie",dial:"255"},
{code:"RW",name:"Rwanda",dial:"250"},
{code:"BI",name:"Burundi",dial:"257"},
{code:"CD",name:"RD Congo",dial:"243"},
{code:"CG",name:"Congo-Brazzaville",dial:"242"},
{code:"CM",name:"Cameroun",dial:"237"},
{code:"CF",name:"Centrafrique",dial:"236"},
{code:"TD",name:"Tchad",dial:"235"},
{code:"GA",name:"Gabon",dial:"241"},
{code:"GQ",name:"Guinée équatoriale",dial:"240"},
{code:"ST",name:"Sao Tomé-et-Principe",dial:"239"},
{code:"AO",name:"Angola",dial:"244"},
{code:"ZM",name:"Zambie",dial:"260"},
{code:"ZW",name:"Zimbabwe",dial:"263"},
{code:"MW",name:"Malawi",dial:"265"},
{code:"MZ",name:"Mozambique",dial:"258"},
{code:"NA",name:"Namibie",dial:"264"},
{code:"BW",name:"Botswana",dial:"267"},
{code:"ZA",name:"Afrique du Sud",dial:"27"},
{code:"LS",name:"Lesotho",dial:"266"},
{code:"SZ",name:"Eswatini",dial:"268"},
{code:"KM",name:"Comores",dial:"269"},
{code:"SC",name:"Seychelles",dial:"248"},
{code:"MU",name:"Maurice",dial:"230"},
{code:"RE",name:"La Réunion",dial:"262"},
{code:"YT",name:"Mayotte",dial:"262"},
{code:"SN",name:"Sénégal",dial:"221"},
{code:"GM",name:"Gambie",dial:"220"},
{code:"GW",name:"Guinée-Bissau",dial:"245"},
{code:"GN",name:"Guinée",dial:"224"},
{code:"SL",name:"Sierra Leone",dial:"232"},
{code:"LR",name:"Liberia",dial:"231"},
{code:"CI",name:"Côte d'Ivoire",dial:"225"},
{code:"GH",name:"Ghana",dial:"233"},
{code:"TG",name:"Togo",dial:"228"},
{code:"BJ",name:"Bénin",dial:"229"},
{code:"NE",name:"Niger",dial:"227"},
{code:"BF",name:"Burkina Faso",dial:"226"},
{code:"ML",name:"Mali",dial:"223"},
{code:"MR",name:"Mauritanie",dial:"222"},
{code:"NG",name:"Nigeria",dial:"234"},
{code:"CV",name:"Cap-Vert",dial:"238"},
{code:"EH",name:"Sahara occidental",dial:"212"},
{code:"SA",name:"Arabie saoudite",dial:"966"},
{code:"AE",name:"Émirats arabes unis",dial:"971"},
{code:"QA",name:"Qatar",dial:"974"},
{code:"KW",name:"Koweït",dial:"965"},
{code:"BH",name:"Bahreïn",dial:"973"},
{code:"OM",name:"Oman",dial:"968"},
{code:"YE",name:"Yémen",dial:"967"},
{code:"JO",name:"Jordanie",dial:"962"},
{code:"LB",name:"Liban",dial:"961"},
{code:"SY",name:"Syrie",dial:"963"},
{code:"IQ",name:"Irak",dial:"964"},
{code:"IL",name:"Israël",dial:"972"},
{code:"PS",name:"Palestine",dial:"970"},
{code:"IR",name:"Iran",dial:"98"},
{code:"TR",name:"Turquie",dial:"90"},
{code:"CN",name:"Chine",dial:"86"},
{code:"JP",name:"Japon",dial:"81"},
{code:"KR",name:"Corée du Sud",dial:"82"},
{code:"KP",name:"Corée du Nord",dial:"850"},
{code:"IN",name:"Inde",dial:"91"},
{code:"PK",name:"Pakistan",dial:"92"},
{code:"BD",name:"Bangladesh",dial:"880"},
{code:"LK",name:"Sri Lanka",dial:"94"},
{code:"NP",name:"Népal",dial:"977"},
{code:"BT",name:"Bhoutan",dial:"975"},
{code:"MM",name:"Myanmar",dial:"95"},
{code:"TH",name:"Thaïlande",dial:"66"},
{code:"VN",name:"Vietnam",dial:"84"},
{code:"KH",name:"Cambodge",dial:"855"},
{code:"LA",name:"Laos",dial:"856"},
{code:"MY",name:"Malaisie",dial:"60"},
{code:"SG",name:"Singapour",dial:"65"},
{code:"ID",name:"Indonésie",dial:"62"},
{code:"PH",name:"Philippines",dial:"63"},
{code:"BN",name:"Brunei",dial:"673"},
{code:"TL",name:"Timor oriental",dial:"670"},
{code:"MN",name:"Mongolie",dial:"976"},
{code:"KZ",name:"Kazakhstan",dial:"7"},
{code:"UZ",name:"Ouzbékistan",dial:"998"},
{code:"TM",name:"Turkménistan",dial:"993"},
{code:"TJ",name:"Tadjikistan",dial:"992"},
{code:"KG",name:"Kirghizistan",dial:"996"},
{code:"AF",name:"Afghanistan",dial:"93"},
{code:"AM",name:"Arménie",dial:"374"},
{code:"AZ",name:"Azerbaïdjan",dial:"994"},
{code:"GE",name:"Géorgie",dial:"995"},
{code:"AU",name:"Australie",dial:"61"},
{code:"NZ",name:"Nouvelle-Zélande",dial:"64"},
{code:"FJ",name:"Fidji",dial:"679"},
{code:"PG",name:"Papouasie-Nouvelle-Guinée",dial:"675"},
{code:"SB",name:"Îles Salomon",dial:"677"},
{code:"VU",name:"Vanuatu",dial:"678"},
{code:"NC",name:"Nouvelle-Calédonie",dial:"687"},
{code:"PF",name:"Polynésie française",dial:"689"},
{code:"WS",name:"Samoa",dial:"685"},
{code:"TO",name:"Tonga",dial:"676"},
{code:"KI",name:"Kiribati",dial:"686"},
{code:"TV",name:"Tuvalu",dial:"688"},
{code:"NR",name:"Nauru",dial:"674"},
{code:"PW",name:"Palaos",dial:"680"},
{code:"FM",name:"Micronésie",dial:"691"},
{code:"MH",name:"Îles Marshall",dial:"692"},
{code:"MX",name:"Mexique",dial:"52"},
{code:"GT",name:"Guatemala",dial:"502"},
{code:"BZ",name:"Belize",dial:"501"},
{code:"HN",name:"Honduras",dial:"504"},
{code:"SV",name:"Salvador",dial:"503"},
{code:"NI",name:"Nicaragua",dial:"505"},
{code:"CR",name:"Costa Rica",dial:"506"},
{code:"PA",name:"Panama",dial:"507"},
{code:"CU",name:"Cuba",dial:"53"},
{code:"JM",name:"Jamaïque",dial:"1876"},
{code:"HT",name:"Haïti",dial:"509"},
{code:"DO",name:"République dominicaine",dial:"1809"},
{code:"BS",name:"Bahamas",dial:"1242"},
{code:"TT",name:"Trinité-et-Tobago",dial:"1868"},
{code:"BB",name:"Barbade",dial:"1246"},
{code:"BR",name:"Brésil",dial:"55"},
{code:"AR",name:"Argentine",dial:"54"},
{code:"CL",name:"Chili",dial:"56"},
{code:"CO",name:"Colombie",dial:"57"},
{code:"PE",name:"Pérou",dial:"51"},
{code:"VE",name:"Venezuela",dial:"58"},
{code:"EC",name:"Équateur",dial:"593"},
{code:"BO",name:"Bolivie",dial:"591"},
{code:"PY",name:"Paraguay",dial:"595"},
{code:"UY",name:"Uruguay",dial:"598"},
{code:"GY",name:"Guyana",dial:"592"},
{code:"SR",name:"Suriname",dial:"597"},
{code:"GF",name:"Guyane française",dial:"594"}
];


// ===================================================
// BARKER - country-select.js
// Sélecteur d'indicatif pays (drapeau + code) pour le formulaire d'inscription.
// ===================================================

function _countryFlag(iso2) {
    // Convertit un code ISO2 ("MG") en emoji drapeau via les "regional indicator symbols" Unicode
    return String.fromCodePoint(...[...iso2.toUpperCase()].map(c => 0x1F1E6 + c.charCodeAt(0) - 65));
}

let _selectedCountry = COUNTRIES[0]; // Madagascar par défaut

function initCountrySelect() {
    const wrap = document.getElementById('countrySelectWrap');
    if (!wrap) return; // n'existe que sur le formulaire d'inscription

    // Pré-sélection : mémorisée localement si déjà choisie une fois sur cet appareil
    try {
        const savedCode = localStorage.getItem('barker_country');
        if (savedCode) {
            const found = COUNTRIES.find(c => c.code === savedCode);
            if (found) _selectedCountry = found;
        }
    } catch (e) {}

    renderCountryButton();
    buildCountryList();

    document.getElementById('countrySelectBtn').addEventListener('click', (e) => {
        e.stopPropagation();
        toggleCountryDropdown();
    });
    document.getElementById('countrySearchInput').addEventListener('input', (e) => {
        filterCountryList(e.target.value);
    });
    document.addEventListener('click', (e) => {
        if (!e.target.closest('#countrySelectWrap')) closeCountryDropdown();
    });

    // Compose le numéro complet (indicatif + numéro local) dans le champ caché
    // envoyé au serveur, juste avant la soumission du formulaire.
    const form = wrap.closest('form');
    if (form) {
        form.addEventListener('submit', () => {
            const localInput = document.getElementById('phoneLocalInput');
            let local = (localInput.value || '').trim().replace(/[\s\-.()]/g, '');
            // Convention internationale classique : on retire le 0 initial du
            // format local avant d'ajouter l'indicatif (ex : 034... -> +261 34...)
            if (local.startsWith('0')) local = local.substring(1);
            document.getElementById('phoneHiddenInput').value = '+' + _selectedCountry.dial + local;
        });
    }
}

function renderCountryButton() {
    const btn = document.getElementById('countrySelectBtn');
    if (!btn) return;
    btn.innerHTML = `${_countryFlag(_selectedCountry.code)} <span style="font-weight:600">+${_selectedCountry.dial}</span> <i class="fas fa-chevron-down" style="font-size:10px;opacity:.6"></i>`;
}

function buildCountryList(filter = '') {
    const listEl = document.getElementById('countryListItems');
    if (!listEl) return;
    const f = filter.trim().toLowerCase();
    const items = COUNTRIES.filter(c => !f || c.name.toLowerCase().includes(f) || c.dial.includes(f));
    listEl.innerHTML = items.map(c => `
        <div class="country-item" data-code="${c.code}">
            <span class="country-item-flag">${_countryFlag(c.code)}</span>
            <span class="country-item-name">${c.name}</span>
            <span class="country-item-dial">+${c.dial}</span>
        </div>`).join('') || `<div style="padding:16px;text-align:center;color:#94a3b8;font-size:13px">Aucun pays trouvé</div>`;

    listEl.querySelectorAll('.country-item').forEach(el => {
        el.addEventListener('click', () => {
            const found = COUNTRIES.find(c => c.code === el.dataset.code);
            if (found) {
                _selectedCountry = found;
                renderCountryButton();
                try { localStorage.setItem('barker_country', found.code); } catch (e) {}
                closeCountryDropdown();
                document.getElementById('phoneLocalInput').focus();
            }
        });
    });
}

function filterCountryList(v) { buildCountryList(v); }

function toggleCountryDropdown() {
    const dd = document.getElementById('countryDropdown');
    const isOpen = dd.style.display === 'flex';
    dd.style.display = isOpen ? 'none' : 'flex';
    if (!isOpen) {
        const search = document.getElementById('countrySearchInput');
        search.value = '';
        buildCountryList('');
        setTimeout(() => search.focus(), 50);
    }
}
function closeCountryDropdown() {
    const dd = document.getElementById('countryDropdown');
    if (dd) dd.style.display = 'none';
}

document.addEventListener('DOMContentLoaded', initCountrySelect);
