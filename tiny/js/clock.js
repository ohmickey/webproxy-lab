const clock = document.querySelector("h2#clock");


function getClock() {
  const date = new Date();
  const hours = date.getHours().toString().padStart(2, "0");
  const minutes = date.getMinutes().toString().padStart(2, "0");
  const seconds = date.getSeconds().toString().padStart(2, "0");
  // 또는 String(date.getHours()).padStart(2,"0");
  // String 으로 감싸서 강제캐스팅도 가능.


  clock.innerText = (`${hours}:${minutes}:${seconds}`);
}
getClock();
setInterval(getClock, 1000);

// setTimeout(sayHello,5000);
// console.log(date.getDate());
// console.log(date.getDay());
// console.log(date.getFullYear());
// console.log(date.getHours());
// console.log(date.getMinutes());
// console.log(date.getSeconds());


