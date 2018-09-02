import { Component, OnInit, OnDestroy } from '@angular/core';
import { DataService } from './shared/data.service';

@Component({
  selector: 'app-root',
  templateUrl: './app.component.html'
})
export class AppComponent implements OnInit, OnDestroy {

  constructor(public data: DataService) {}

  ngOnInit() {
    this.data.initSocketEvents();
  }

  ngOnDestroy() {
    this.data.deinitSocketEvents();
  }
}
