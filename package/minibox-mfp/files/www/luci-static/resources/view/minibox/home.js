'use strict';
'require view';

return view.extend({
	render: function() {
		return E('div', { 'class': 'cbi-map' }, [
			E('h2', {}, [ _('MiniBox print + scan') ]),
			E('div', { 'class': 'cbi-section' }, [
				E('p', {}, [ _('Вбудована панель стану принтера, сканера та мережевого виявлення.') ]),
				E('p', {}, [
					E('a', {
						'class': 'btn cbi-button cbi-button-action important',
						'href': '/'
					}, [ _('Відкрити MiniWeb') ])
				])
			])
		]);
	}
});
