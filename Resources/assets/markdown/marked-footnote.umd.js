/*! marked-footnote v1.4.0 | MIT License | https://github.com/bent10/marked-extensions/tree/main/packages/footnote */
(function(h,$){typeof exports=="object"&&typeof module<"u"?module.exports=$():typeof define=="function"&&define.amd?define($):(h=typeof globalThis<"u"?globalThis:h||self,h.markedFootnote=$())})(this,function(){"use strict";function h(e,d){const l={type:"footnotes",raw:d,rawItems:[],items:[]};return{name:"footnote",level:"block",childTokens:["content"],tokenizer(i){e.hasFootnotes||(this.lexer.tokens.push(l),e.tokens=this.lexer.tokens,e.hasFootnotes=!0,l.rawItems=[],l.items=[]);const a=/^\[\^([^\]\n]+)\]:(?:[ \t]+|[\n]*?|$)([^\n]*?(?:\n|$)(?:\n*?[ ]{4,}[^\n]*)*)/.exec(i);if(a){const[f,c,s=""]=a;let o=s.split(`
`).reduce((r,u)=>r+`
`+u.replace(/^(?:[ ]{4}|[\t])/,""),"");const t=o.trimEnd().split(`
`).pop();o+=t&&/^[ \t]*?[>\-*][ ]|[`]{3,}$|^[ \t]*?[|].+[|]$/.test(t)?`

`:"";const n={type:"footnote",raw:f,label:c,refs:[],content:this.lexer.blockTokens(o)};return l.rawItems.push(n),n}},renderer(){return""}}}function $(e){return e.replaceAll("&","&amp;").replaceAll("<","&lt;").replaceAll(">","&gt;")}function T(e,d=!1,l=!1){let i=0;return{name:"footnoteRef",level:"inline",tokenizer(a){const f=/^\[\^([^\]\n]+)\]/.exec(a);if(f){const[c,s]=f,o=this.lexer.tokens[0],t=o.rawItems.filter(p=>p.label===s);if(!t.length)return;const n=t[0],r=o.items.filter(p=>p.label===s)[0],u={type:"footnoteRef",raw:c,index:n.refs.length,id:"",label:s};return r?(u.id=r.refs[0].id,r.refs.push(u)):(i++,u.id=String(i),n.refs.push(u),o.items.push(n)),u}},renderer({index:a,id:f,label:c}){i=0;const s=encodeURIComponent(c),o=l?$(c):f,t=a>0?`-${a+1}`:"";return`<sup><a id="${e}ref-${s}${t}" href="#${e+s}" data-${e}ref aria-describedby="${e}label">${d?`[${o}]`:o}</a></sup>`}}}function g(e,d,l,i,a,f){return{name:"footnotes",renderer({raw:c,items:s=[]}){if(s.length===0)return"";const o=s.reduce((u,{label:p,content:A,refs:E})=>{const w=encodeURIComponent(p),k=this.parser.parse(A).trimEnd(),x=k.endsWith("</p>");let m=`<li id="${e+w}">
`;return m+=x?k.replace(/<\/p>$/,""):k,E.forEach((C,L)=>{const I=f.replace("{0}",p);let b,F;if(L>0){const y=L+1;b=`↩<sup>${y}</sup>`,F=`-${y}`}else b="↩",F="";m+=` <a href="#${e}ref-${w}${F}" data-${e}backref aria-label="${I}">${b}</a>`}),m+=x?`</p>
`:`
`,m+=`</li>
`,u+m},"");let t="";l&&(t+=`<hr data-${d}footnotes>
`);let n="";i&&(n=` class="${i}"`);let r="";return a&&(r=` class="${a}"`),t+=`<section${n} data-${d}footnotes>
`,t+=`<h2 id="${e}label"${r}>${c.trimEnd()}</h2>
`,t+=`<ol>
${o}</ol>
`,t+=`</section>
`,t}}}function R(e={}){const{prefixId:d="footnote-",prefixData:l="",description:i="Footnotes",refMarkers:a=!1,footnoteDivider:f=!1,keepLabels:c=!1,sectionClass:s="footnotes",headingClass:o="sr-only",backRefLabel:t="Back to reference {0}"}=e,n={hasFootnotes:!1,tokens:[]};return{extensions:[h(n,i),T(d,a,c),g(d,l,f,s,o,t)],walkTokens(r){r.type==="footnotes"&&n.tokens.indexOf(r)===0&&r.items.length&&(n.tokens[0]={type:"space",raw:""},n.tokens.push(r)),n.hasFootnotes&&(n.hasFootnotes=!1)}}}return R});
//# sourceMappingURL=index.umd.cjs.map
