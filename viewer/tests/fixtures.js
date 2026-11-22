export function fixture(name, mip) {
  const props = ['x','y','z',...Array.from({length:3},(_,i)=>`f_dc_${i}`),
    ...Array.from({length:45},(_,i)=>`f_rest_${i}`),'opacity',
    ...Array.from({length:3},(_,i)=>`scale_${i}`),...Array.from({length:4},(_,i)=>`rot_${i}`)];
  const header = `ply\nformat binary_little_endian 1.0\ncomment SplatRenderMode: ${mip ? 'mip' : 'default'}\nelement vertex 512\n`
    + props.map(p=>`property float ${p}\n`).join('') + 'end_header\n';
  const body = new ArrayBuffer(512*props.length*4), view = new DataView(body);
  for (let i=0;i<512;i++) {
    const row=[i%8*.1,Math.floor(i/8)%8*.1,Math.floor(i/64)*.1,
      ...Array.from({length:3},(_,j)=>Math.sin(i*.1+j)*.1),
      ...Array.from({length:45},(_,j)=>Math.sin(i*.1+j)*.001),1,-4,-4,-4,1,0,0,0];
    row.forEach((v,j)=>view.setFloat32((i*props.length+j)*4,v,true));
  }
  return new File([header,body],name);
}
