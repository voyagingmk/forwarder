const base64 = require('./base64');

const buf = new Buffer("\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\");
const b64 = base64.fromByteArray(buf);

console.log("b64", b64);

const ret = base64.toByteArray(b64);
//console.log("ret", ret);