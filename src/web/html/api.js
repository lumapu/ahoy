/**
 * SVG ICONS
 */

iconWifi1 = [
    "M11.046 10.454c.226-.226.185-.605-.1-.75A6.473 6.473 0 0 0 8 9c-1.06 0-2.062.254-2.946.704-.285.145-.326.524-.1.75l.015.015c.16.16.407.19.611.09A5.478 5.478 0 0 1 8 10c.868 0 1.69.201 2.42.56.203.1.45.07.611-.091l.015-.015zM9.06 12.44c.196-.196.198-.52-.04-.66A1.99 1.99 0 0 0 8 11.5a1.99 1.99 0 0 0-1.02.28c-.238.14-.236.464-.04.66l.706.706a.5.5 0 0 0 .707 0l.708-.707z"
];

iconWifi2 = [
    "M13.229 8.271c.216-.216.194-.578-.063-.745A9.456 9.456 0 0 0 8 6c-1.905 0-3.68.56-5.166 1.526a.48.48 0 0 0-.063.745.525.525 0 0 0 .652.065A8.46 8.46 0 0 1 8 7a8.46 8.46 0 0 1 4.577 1.336c.205.132.48.108.652-.065zm-2.183 2.183c.226-.226.185-.605-.1-.75A6.473 6.473 0 0 0 8 9c-1.06 0-2.062.254-2.946.704-.285.145-.326.524-.1.75l.015.015c.16.16.408.19.611.09A5.478 5.478 0 0 1 8 10c.868 0 1.69.201 2.42.56.203.1.45.07.611-.091l.015-.015zM9.06 12.44c.196-.196.198-.52-.04-.66A1.99 1.99 0 0 0 8 11.5a1.99 1.99 0 0 0-1.02.28c-.238.14-.236.464-.04.66l.706.706a.5.5 0 0 0 .708 0l.707-.707z"
];

iconWifi3 = [
    "M15.384 6.115a.485.485 0 0 0-.047-.736A12.444 12.444 0 0 0 8 3C5.259 3 2.723 3.882.663 5.379a.485.485 0 0 0-.048.736.518.518 0 0 0 .668.05A11.448 11.448 0 0 1 8 4c2.507 0 4.827.802 6.716 2.164.205.148.49.13.668-.049z",
    "M13.229 8.271a.482.482 0 0 0-.063-.745A9.455 9.455 0 0 0 8 6c-1.905 0-3.68.56-5.166 1.526a.48.48 0 0 0-.063.745.525.525 0 0 0 .652.065A8.46 8.46 0 0 1 8 7a8.46 8.46 0 0 1 4.576 1.336c.206.132.48.108.653-.065zm-2.183 2.183c.226-.226.185-.605-.1-.75A6.473 6.473 0 0 0 8 9c-1.06 0-2.062.254-2.946.704-.285.145-.326.524-.1.75l.015.015c.16.16.407.19.611.09A5.478 5.478 0 0 1 8 10c.868 0 1.69.201 2.42.56.203.1.45.07.61-.091l.016-.015zM9.06 12.44c.196-.196.198-.52-.04-.66A1.99 1.99 0 0 0 8 11.5a1.99 1.99 0 0 0-1.02.28c-.238.14-.236.464-.04.66l.706.706a.5.5 0 0 0 .707 0l.707-.707z"
];

iconWarn = [
    "M7.938 2.016A.13.13 0 0 1 8.002 2a.13.13 0 0 1 .063.016.146.146 0 0 1 .054.057l6.857 11.667c.036.06.035.124.002.183a.163.163 0 0 1-.054.06.116.116 0 0 1-.066.017H1.146a.115.115 0 0 1-.066-.017.163.163 0 0 1-.054-.06.176.176 0 0 1 .002-.183L7.884 2.073a.147.147 0 0 1 .054-.057zm1.044-.45a1.13 1.13 0 0 0-1.96 0L.165 13.233c-.457.778.091 1.767.98 1.767h13.713c.889 0 1.438-.99.98-1.767L8.982 1.566z",
    "M7.002 12a1 1 0 1 1 2 0 1 1 0 0 1-2 0zM7.1 5.995a.905.905 0 1 1 1.8 0l-.35 3.507a.552.552 0 0 1-1.1 0L7.1 5.995z"
];

iconInfo = [
    "M8 15A7 7 0 1 1 8 1a7 7 0 0 1 0 14zm0 1A8 8 0 1 0 8 0a8 8 0 0 0 0 16z",
    "m8.93 6.588-2.29.287-.082.38.45.083c.294.07.352.176.288.469l-.738 3.468c-.194.897.105 1.319.808 1.319.545 0 1.178-.252 1.465-.598l.088-.416c-.2.176-.492.246-.686.246-.275 0-.375-.193-.304-.533L8.93 6.588zM9 4.5a1 1 0 1 1-2 0 1 1 0 0 1 2 0z"
];

iconSuccess = [
    "M8 15A7 7 0 1 1 8 1a7 7 0 0 1 0 14zm0 1A8 8 0 1 0 8 0a8 8 0 0 0 0 16z",
    "M10.97 4.97a.235.235 0 0 0-.02.022L7.477 9.417 5.384 7.323a.75.75 0 0 0-1.06 1.06L6.97 11.03a.75.75 0 0 0 1.079-.02l3.992-4.99a.75.75 0 0 0-1.071-1.05z"
];

 /**
 * GENERIC FUNCTIONS
 */
function ml(tagName, ...args) {
    var el = document.createElement(tagName);
    if(args[0]) {
        for(var name in args[0]) {
            if(name.indexOf("on") === 0) {
                el.addEventListener(name.substr(2).toLowerCase(), args[0][name], false)
            } else {
                el.setAttribute(name, args[0][name]);
            }
        }
    }
    if (!args[1]) {
        return el;
    }
    return nester(el, args[1])
}

function nester(el, n) {
    if (typeof n === "string") {
        var t = document.createTextNode(n);
        el.appendChild(t);
    } else if (n instanceof Array) {
        for(var i = 0; i < n.length; i++) {
            if (typeof n[i] === "string") {
                var t = document.createTextNode(n[i]);
                el.appendChild(t);
            } else if (n[i] instanceof Node){
                el.appendChild(n[i]);
            }
        }
    } else if (n instanceof Node){
        el.appendChild(n)
    }
    return el;
}

function topnav() {
    toggle("topnav", "mobile");
}

function parseNav(obj) {
    for(i = 0; i < 11; i++) {
        if(i == 2)
            continue;
        var l = document.getElementById("nav"+i);
        if(window.location.pathname == "/" + l.href.split('/').pop())
            l.classList.add("active");

        if(obj["menu_protEn"]) {
            if(obj["menu_prot"]) {
                if(0 == i)
                    l.classList.remove("hide");
                else if(i > 2) {
                    if(((obj["menu_mask"] >> (i-2)) & 0x01) == 0x00)
                        l.classList.remove("hide");
                }
            } else if(0 != i)
                l.classList.remove("hide");
        } else if(i > 1)
            l.classList.remove("hide");
    }
}

function parseVersion(obj) {
    document.getElementById("version").appendChild(
        link("https://github.com/lumapu/ahoy/commits/" + obj["build"], "Git SHA: " + obj["build"] + " :: " + obj["version"], "_blank")
    );
}

function parseESP(obj) {
    document.getElementById("esp_type").replaceChildren(
        document.createTextNode("Board: " + obj["esp_type"])
    );
}

function parseRssi(obj) {
    var icon = iconWifi3;
    if(obj["wifi_rssi"] <= -80)
        icon = iconWifi1;
    else if(obj["wifi_rssi"] <= -70)
        icon = iconWifi2;
    document.getElementById("wifiicon").replaceChildren(svg(icon, 32, 32, "wifi", obj["wifi_rssi"]));
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

function toggle(id, cl="hide") {
    var e = document.getElementById(id);
    if(!e.classList.contains(cl))
        e.classList.add(cl);
    else
        e.classList.remove(cl);
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

function inp(name, val, max=32, cl=["text"], id=null, type=null, pattern=null, title=null, checked=null) {
    e = document.createElement('input');
    e.classList.add(...cl);
    e.name = name;
    if(null != val) e.value = val;
    if(null != max) e.maxLength = max;
    if(null != id) e.id = id;
    if(null != type) e.type = type;
    if(null != pattern) e.pattern = pattern;
    if(null != title) e.title = title;
    if(null != checked) e.checked = checked;
    return e;
}

function sel(name, options, selId) {
    e = document.createElement('select');
    e.name = name;
    for(it of options) {
        o = opt(it[0], it[1], (it[0] == selId));
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

function opt(val, html, sel=false) {
    o = document.createElement('option');
    o.value = val;
    o.innerHTML = html;
    if(sel)
        o.selected = true;
    return o;
}

function div(cl, h=null) {
    e = document.createElement('div');
    e.classList.add(...cl);
    if(null != h) e.innerHTML = h;
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

function svg(data=null, w=24, h=24, cl=null, tooltip=null) {
    var s = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    s.setAttribute('width', w);
    s.setAttribute('height', h);
    s.setAttribute('viewBox', '0 0 16 16');
    if(null != cl) s.setAttribute('class', cl);
    if(null != data) {
        for(const e of data) {
            const i = document.createElementNS('http://www.w3.org/2000/svg', 'path');
            i.setAttribute('d', e);
            s.appendChild(i);
        }
    }
    if(null != tooltip) {
        const t = document.createElement("title");
        t.appendChild(document.createTextNode(tooltip));
        s.appendChild(t);
    }
    return s;
}
