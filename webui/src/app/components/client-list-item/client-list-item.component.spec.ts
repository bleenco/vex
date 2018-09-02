import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { ClientListItemComponent } from './client-list-item.component';

describe('ClientListItemComponent', () => {
  let component: ClientListItemComponent;
  let fixture: ComponentFixture<ClientListItemComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [ ClientListItemComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(ClientListItemComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
