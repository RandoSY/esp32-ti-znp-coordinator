'use strict';
// Offline: published installed parsers/adapter/publisher; no Controller.start,
// broker connections, serial handles, live config reads or package mutations.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),os=require('node:os');
const root=path.resolve(__dirname,'..');
const tmp=fs.mkdtempSync(path.join(os.tmpdir(),'znp-lqi-host-'));
process.env.ZIGBEE2MQTT_DATA=tmp;
fs.writeFileSync(path.join(tmp,'configuration.yaml'),JSON.stringify({version:5,mqtt:{server:'mqtt://127.0.0.1'},advanced:{cache_state:true,last_seen:'disable',output:'json'}}));
const installed=path.join(root,'build/zigbee2mqtt');
const h=path.join(installed,'node_modules/zigbee-herdsman/dist');
// Empty ASDUs intentionally test AF wire boundaries, not ZCL validity.
// Silence expected short-header debug messages in this isolated process only.
require(path.join(h,'utils/logger.js')).setLogger({debug(){},info(){},warning(){},error(){}});
const {Frame}=require(path.join(h,'adapter/z-stack/unpi/frame.js'));
const {ZpiObject}=require(path.join(h,'adapter/z-stack/znp/zpiObject.js'));
const {ZStackAdapter}=require(path.join(h,'adapter/z-stack/adapter/zStackAdapter.js'));
function parse(hex){const bytes=Buffer.from(hex,'hex');return ZpiObject.fromUnpiFrame(Frame.fromBuffer(bytes[1],bytes.length-1,bytes));}
function adapter(object){let emitted;ZStackAdapter.prototype.onZnpRecieved.call({waitress:{resolve(){}},emit:(name,value)=>{assert.equal(name,'zclPayload');emitted=value;}},object);assert(emitted);return emitted;}
(async()=>{
    const entries=fs.readFileSync(process.argv[2],'utf8').trim().split('\n').map(x=>JSON.parse(x));
    for(const c of entries){
        const o=parse(c.hex),p=o.payload;
        assert.equal(o.command.name,'incomingMsg');
        assert.deepEqual([p.groupid,p.clusterid,p.srcaddr,p.srcendpoint,p.dstendpoint,p.wasbroadcast,p.linkquality,p.securityuse,p.timestamp,p.transseqnumber,p.len],[0x1234,0x0500,0xBEEF,3,11,1,c.expected,1,0x78563412,1,c.length]);
        assert.deepEqual([...p.data],Array.from({length:c.length},(_,i)=>(i*37+9)&255));
        const e=adapter(o);assert.equal(e.linkquality,c.expected);assert.equal(e.endpoint,3);assert.equal(e.destinationEndpoint,11);assert.equal(e.address,0xBEEF);
    }
    // Independent fixed wire fixtures follow TI's standard/extended layouts.
    // EXT fixtures test installed-host parsing only: firmware does not emit EXT.
    const gold=JSON.parse(fs.readFileSync(path.join(__dirname,'fixtures/znp_af_lqi.json')));
    for(const c of gold.cases){
        const o=parse(c.hex);assert.equal(o.command.name,c.command);assert.equal(o.payload.linkquality,c.lqi);
        const e=adapter(o);assert.equal(e.linkquality,c.lqi);assert.equal(e.address,0xBEEF);assert.deepEqual([...e.data],[9,0x42,0]);
        const corrupt=Buffer.from(c.hex,'hex');corrupt[corrupt.length-1]^=1;assert.throws(()=>parse(corrupt.toString('hex')),/checksum/);
    }
    // Actual unmodified Z2M publication method with inert state/MQTT endpoints.
    const {Controller}=require(path.join(installed,'dist/controller.js'));
    const State=require(path.join(installed,'dist/state.js')).default;
    const events={emitStateChange(){},emitPublishEntityState(){}};
    const sent=[];const fake={state:new State(events,{}),extensions:[],eventBus:events,mqtt:{publish:async(topic,json)=>sent.push(JSON.parse(json))}};
    const entity={ID:'0x0000000000001234',name:'synthetic_door',isDevice:()=>true,options:{},zh:{}};
    const publish=Object.getOwnPropertyDescriptor(Controller.prototype,'publishEntityState').get.call(fake);
    for(const lqi of [0,1,15,30,142,173,254,255]){entity.zh.linkquality=lqi;await publish(entity,{contact:false});assert.equal(sent.at(-1).linkquality,lqi);assert.equal(sent.at(-1).contact,false);}
    // Latest stored device metadata wins at publication, even when a payload
    // was prepared before a later receive. This is not a packet-bound RSSI.
    entity.zh.linkquality=173;const delayed={contact:true};entity.zh.linkquality=0;
    await publish(entity,delayed);assert.equal(sent.at(-1).linkquality,0);
    entity.zh.linkquality=undefined;await publish(entity,{contact:false});assert.equal(sent.at(-1).linkquality,undefined);
    const fz=require(path.join(installed,'node_modules/zigbee-herdsman-converters/dist/converters/fromZigbee.js'));
    for(const status of [0,1]){const p=fz.ias_contact_alarm_1.convert({}, {data:{zonestatus:status}},()=>{},{},{});assert.equal(p.contact,status===0);assert.equal(p.linkquality,undefined);}
    console.log(`PASS: ${entries.length} production frames parsed by installed herdsman 10.10.0; standard/EXT golden fixtures, FCS rejection, adapter passthrough, Z2M 2.14.2 publisher zero/boundaries/latest-value behavior, IAS converter`);
})().catch(e=>{console.error(e.stack);process.exitCode=1;}).finally(()=>{
    // Delete only the exact temporary directory created above.
    for(const f of fs.readdirSync(tmp))fs.unlinkSync(path.join(tmp,f));fs.rmdirSync(tmp);
});
