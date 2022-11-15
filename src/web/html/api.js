/**
 * GENERIC FUNCTIONS
 */

function topnav() {
    toggle("topnav");
}

function parseMenu(obj) {
    var e = document.getElementById("topnav");
    e.innerHTML = "";
    for(var i = 0; i < obj["name"].length; i ++) {
        if(obj["name"][i] == "-")
            e.appendChild(span("", ["seperator"]));
        else {
            var l = link(obj["link"][i], obj["name"][i], obj["trgt"][i]);
            if(obj["link"][i] == window.location.pathname)
                l.classList.add("active");
            e.appendChild(l);
        }
    }
}

function parseVersion(obj) {
    document.getElementById("version").appendChild(
        link("https://github.com/lumapu/ahoy/commits/" + obj["build"], "Git SHA: " + obj["build"] + " :: " + obj["version"], "_blank")
    );
}

function parseESP(obj) {
    document.getElementById("esp_type").innerHTML="Board: " + obj["esp_type"];
}

function parseSysInfo(obj) {
    document.getElementById("sdkversion").innerHTML=    "SDKv.: " + obj["sdk_version"];
    document.getElementById("cpufreq").innerHTML=       "CPU MHz: " + obj["cpu_freq"] + "MHz";
    document.getElementById("chiprevision").innerHTML=  "Rev.: " + obj["chip_revision"];
    document.getElementById("chipmodel").innerHTML=     "Model: " + obj["chip_model"];
    document.getElementById("chipcores").innerHTML=     "Core: " + obj["chip_cores"];
    document.getElementById("esp_type").innerHTML=      "Type: " + obj["esp_type"];

    document.getElementById("heap_used").innerHTML=     "Used: " + obj["heap_used"];
    document.getElementById("heap_total").innerHTML=      "Total: " + obj["heap_total"];
}

function changeProgressbar(id, value, max) {
    document.getElementById(id).value = value;
    document.getElementById(id).max = max;
}

function setHide(id, hide) {
    var elm = document.getElementById(id);
    if(hide) {
        if(!elm.classList.contains("hide"))
            elm.classList.add("hide");
    }
    else
        elm.classList.remove('hide');
}


function toggle(id) {
    var e = document.getElementById(id);
    if(!e.classList.contains("hide"))
        e.classList.add("hide");
    else
        e.classList.remove('hide');
}

function getAjax(url, ptr, method="GET", json=null) {
    var xhr = new XMLHttpRequest();
    if(xhr != null) {
       xhr.open(method, url, true);
       xhr.onreadystatechange = p;
       if("POST" == method)
            xhr.setRequestHeader("Content-Type", "application/json;charset=UTF-8");
       xhr.send(json);
    }
    function p() {
        if(xhr.readyState == 4) {
            if(null != xhr.responseText) {
                if(null != ptr)
                    ptr(JSON.parse(xhr.responseText));
            }
        }
    }
}

/**
 * CREATE DOM FUNCTIONS
 */

function des(val) {
    e = document.createElement('p');
    e.classList.add("subdes");
    e.innerHTML = val;
    return e;
}

function lbl(htmlfor, val, cl=null, id=null) {
    e = document.createElement('label');
    e.htmlFor = htmlfor;
    e.innerHTML = val;
    if(null != cl) e.classList.add(...cl);
    if(null != id) e.id = id;
    return e;
}

function inp(name, val, max=32, cl=["text"], id=null, type=null) {
    e = document.createElement('input');
    e.classList.add(...cl);
    e.name = name;
    e.value = val;
    if(null != max) e.maxLength = max;
    if(null != id) e.id = id;
    if(null != type) e.type = type;
    return e;
}

function sel(name, opt, selId) {
    e = document.createElement('select');
    e.name = name;
    for(it of opt) {
        o = document.createElement('option');
        o.value = it[0];
        o.innerHTML = it[1];
        if(it[0] == selId)
            o.selected = true;
        e.appendChild(o);
    }
    return e;
}

function selDelAllOpt(sel) {
    var i, l = sel.options.length - 1;
    for(i = l; i >= 0; i--) {
        sel.remove(i);
    }
}

function opt(val, html) {
    o = document.createElement('option');
    o.value = val;
    o.innerHTML = html;
    e.appendChild(o);
    return o;
}

function div(cl) {
    e = document.createElement('div');
    e.classList.add(...cl);
    return e;
}

function span(val, cl=null, id=null) {
    e = document.createElement('span');
    e.innerHTML = val;
    if(null != cl) e.classList.add(...cl);
    if(null != id) e.id = id;
    return e;
}

function br() {
    return document.createElement('br');
}

function link(dst, text, target=null) {
    var a = document.createElement('a');
    var t = document.createTextNode(text);
    a.href = dst;
    if(null != target)
        a.target = target;
    a.appendChild(t);
    return a;
}
