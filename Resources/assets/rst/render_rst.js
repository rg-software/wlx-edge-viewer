function renderRST(node) {
	if (!node) return '';
	if (Array.isArray(node)) return node.map(renderRST).join('');
  
	const childrenHTML = renderRST(node.children);
  
	switch (node.type) {
	  case 'document':
	  case 'section': return childrenHTML;
	  case 'paragraph': return `<p>${childrenHTML}</p>`;
	  case 'text': return node.value;
	  case 'emphasis': return `<em>${childrenHTML}</em>`;
	  case 'strong': return `<strong>${childrenHTML}</strong>`;
	  case 'literal': return `<code>${childrenHTML}</code>`;
	  case 'literal_block': return `<pre>${childrenHTML}</pre>`;
	  case 'block_quote': return `<blockquote>${childrenHTML}</blockquote>`;
	  case 'bullet_list': return `<ul>${childrenHTML}</ul>`;
	  case 'enumerated_list': return `<ol>${childrenHTML}</ol>`;
	  case 'list_item': return `<li>${childrenHTML}</li>`;
	  case 'title': return `<h1>${childrenHTML}</h1>`;
	  case 'subtitle': return `<h2>${childrenHTML}</h2>`;
	  case 'field_list': return `<dl>${childrenHTML}</dl>`;
	  case 'field':
		return `<dt>${renderRST(node.children[0])}</dt><dd>${renderRST(node.children[1])}</dd>`;
	  case 'reference':
		return `<a href="${node.url}">${childrenHTML}</a>`;
	  case 'note':
		return `<div class="admonition note"><strong>Note:</strong> ${childrenHTML}</div>`;
	  case 'warning':
		return `<div class="admonition warning"><strong>Warning:</strong> ${childrenHTML}</div>`;
	  case 'directive':
		return `<div class="directive ${node.name}"><strong>${node.name}:</strong> ${childrenHTML}</div>`;
	  case 'footnote_reference':
		return `<sup class="footnote-ref">[${node.ref}]</sup>`;
	  case 'footnote':
		return `<div class="footnote"><sup>[${node.label}]</sup> ${childrenHTML}</div>`;
	  case 'transition':
		return `<hr class="transition">`;
	  case 'comment':
		return ''; // Do not render comments
	  default:
		console.warn(`Unhandled node type: ${node.type}`);
		return childrenHTML;
	}
  }
  
  