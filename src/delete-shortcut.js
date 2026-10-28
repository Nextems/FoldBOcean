export function installDeleteShortcut(container,{getSelection,requestDelete,beforeDelete=()=>{}}){
 container.addEventListener('keydown',event=>{
  if(event.key!=='Delete'||event.repeat||event.ctrlKey||event.altKey||event.metaKey)return;
  if(event.target.closest('input,textarea,select,[contenteditable]:not([contenteditable="false"])')||document.querySelector('dialog[open]'))return;
  const ids=[...getSelection()].filter(Boolean);if(!ids.length)return;
  event.preventDefault();event.stopPropagation();beforeDelete();requestDelete(ids);
 });
}
